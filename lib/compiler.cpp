#include <iostream>
#include "sprite/curryinput.hpp"
#include "sprite/compiler.hpp"

using namespace sprite;
using namespace sprite::compiler::member_labels; // for ND_* and VT_* enums.
namespace tgt = sprite::backend;

namespace
{
  // Looks up a node symbol table from the library.
  inline compiler::NodeSTab const &
  lookup(compiler::LibrarySTab const & table, curry::Qname const & qname)
    { return table.modules.at(qname.module).nodes.at(qname.name); }

  /**
   * @brief Composes an expression by rewriting the given root.
   *
   * If the expression contains variable references, then they are
   * resolved relative to the root, in which case the function
   * definition must be supplied.
   */
  struct Rewriter
  {
    using result_type = void;

    Rewriter(
        compiler::ModuleCompiler const & compiler_
      , tgt::value const & root_p_
      , curry::Function const * fundef_ = nullptr
      )
      : compiler(compiler_)
      , root_p(bitcast(root_p_, *compiler_.ir.node_t))
      , target_p(root_p)
      , fundef(fundef_)
    {}

    compiler::ModuleCompiler const & compiler;

    // A pointer in the target to the root node undergoing pattern matching
    // or rewriting.
    tgt::value root_p;

    // The current node that will be rewritten.  When constructing nested
    // expressions, root_p will never change, but the target will change to
    // point to the subexpression currently under construction.
    tgt::value target_p;

    // The definition of the function being compiled.
    curry::Function const * fundef;

    // Looks up a node using the function paths and path reference.  Returns
    // the result as an i8* in the target.
    tgt::value resolve_path(size_t pathid) const
    {
      tgt::value p = this->root_p;
      _resolve_path(p, this->fundef->paths.at(pathid));
      return bitcast(p, *tgt::types::char_());
    }

    // Helper for resolve_path.  Updates p.
    void _resolve_path(
        tgt::value & p, curry::Function::PathElem const & pathelem
      ) const
    {
      // Walk the base path.
      if(pathelem.base != curry::nobase)
        _resolve_path(p, this->fundef->paths.at(pathelem.base));

      // Add this index.
      tgt::type node_pt = *this->compiler.ir.node_t;
      switch(pathelem.idx)
      {
        case 0:
          p = bitcast(p.arrow(ND_SLOT0), node_pt);
          break;
        case 1:
          p = bitcast(p.arrow(ND_SLOT1), node_pt);
          break;
        default: assert(0 && "Can't do arity>2 yet"); // FIXME
      }
    }

    result_type operator()(curry::Rule const & rule)
      { rule.visit(*this); }

    // Subcases of Rule.
    result_type operator()(int rule)
      {} // Not implemented.

    result_type operator()(double rule)
      {} // Not implemented.

    // Rewrites the root as a FWD node.
    result_type operator()(curry::Ref rule)
    {
      tgt::value target = this->resolve_path(rule.pathid);
      this->target_p.arrow(ND_VPTR) = tgt::scope::current_module().getglobal(".fwd.vt");
      this->target_p.arrow(ND_TAG) = compiler::FWD;
      this->target_p.arrow(ND_SLOT0) = target;
    }

    result_type operator()(curry::Expr const & expr)
    {
      // The target node pointer is clobbered so that recursion can be used.
      // Save it for below.
      tgt::value orig_target_p = this->target_p;

      // Each child needs to be allocated and initialized.  This children are
      // stored as i8* pointers.
      std::vector<tgt::value> children;
      children.reserve(expr.args.size());
      for(auto const & subexpr: expr.args)
      {
        // Avoid creating FWD nodes for subexpressions.
        if(curry::Ref const * varref = subexpr.getvar())
        {
          // Retrieve the pointer to an existing node.
          tgt::value child = this->resolve_path(varref->pathid);
          children.push_back(child);
        }
        // Allocate a new node and place its contents with a recursive call.
        else
        {
          tgt::value child = this->compiler.node_alloc();
          children.push_back(child);
          // Clobber the root so the recursive call works.
          this->target_p = bitcast(child, *this->compiler.ir.node_t);
          (*this)(subexpr);
        }
      }

      // Restore the original target.
      this->target_p = orig_target_p;

      // Set the vtable and tag.
      this->compiler.node_init(
          this->target_p, lookup(compiler.lib_stab, expr.qname)
        );

      // Set the child pointers.
      assert(children.size() <= 2 && "n>2 Not implemented");
      for(size_t i=0; i<children.size(); ++i)
        this->target_p.arrow(ND_SLOT0+i) = children[i];
    }
  };

  struct FunctionCompiler : Rewriter
  {
    using result_type = void;
    using Rewriter::Rewriter;

  private:

    // A pointer in the target to the current inductive position.
    tgt::value inductive = nullptr;

  public:

    result_type operator()(curry::Branch const & branch)
    {
      std::vector<tgt::label> labels{
          tgt::label([&]{
              this->compiler.clib.printf("FAIL case hit.\n");
              tgt::return_().set_metadata("case...FAIL");
            })
        , tgt::label([&]{
              this->compiler.clib.printf("FWD case hit.\n");
              tgt::return_().set_metadata("case...FWD");
            })
        , tgt::label([&]{
              this->compiler.clib.printf("CHOICE case hit.\n");
              tgt::return_().set_metadata("case...CHOICE");
            })
        , tgt::label([&]{
              this->compiler.clib.printf("OPER case hit.\n");
              tgt::return_().set_metadata("case...OPER");
            })
        };
      for(auto const & case_: branch.cases)
      {
        // Allocate a label, then recursively generate the next step in its
        // scope.
        tgt::label tmp;
        tgt::scope _ = tmp;
        case_.action.visit(*this);
        labels.push_back(tmp);
      }
      std::vector<tgt::block_address> addresses;
      for(tgt::label const & l: labels)
        addresses.push_back(&l);

      // Declare the jump table in the target program.
      tgt::type char_p = *tgt::types::char_();
      tgt::globalvar jumptable =
          tgt::static_(char_p[labels.size()], tgt::flexible(".jtable"))
              .set_initializer(addresses);
      // tgt::ref jump = &jumptable[4];

      // Look up the inductive node.
      this->inductive = bitcast(
          this->resolve_path(branch.pathid), *this->compiler.ir.node_t
        );

      // Use the tag of the inductive node to make a jump.
      tgt::value index = this->inductive.arrow(ND_TAG) + 4;
      tgt::goto_(jumptable[index], labels);
    }

    result_type operator()(curry::Rule const & rule)
    {
      // Trace.
      // compiler.clib.printf("S> --- ");
      // compiler.rt.printexpr(target_p, "\n");
      // compiler.clib.fflush(nullptr);

      // Step.
      static_cast<Rewriter*>(this)->operator()(rule);

      // Trace.
      // compiler.clib.printf("S> +++ ");
      // compiler.rt.printexpr(target_p, "\n");
      // compiler.clib.fflush(nullptr);
      tgt::return_();
    }
  };

  // Helper to simplify the use of FunctionCompiler.
  void compile_function(
      compiler::ModuleCompiler const & compiler
    , tgt::value const & root_p
    , curry::Function const & fun
    )
  {
    ::FunctionCompiler c(compiler, root_p, &fun);
    return fun.def.visit(c);
  }

  // Makes a function that returns the given static C-string.  Returns a
  // pointer to that function.
  tgt::constant make_name_func(
      compiler::ModuleCompiler const & compiler, std::string const & name
    )
  {
    tgt::function f = tgt::static_<tgt::function>(
        compiler.ir.labelfun_t, tgt::flexible(".name"), {}
      , [&] { tgt::return_(name); }
      );
    return &f;
  }

  // Makes a function that returns the given integer.  Returns a
  // pointer to that function.
  tgt::constant make_arity_func(
      compiler::ModuleCompiler const & compiler, int64_t arity
    )
  {
    tgt::function f = tgt::static_<tgt::function>(
        compiler.ir.arityfun_t, tgt::flexible(".arity"), {}
      , [&] { tgt::return_(arity); }
      );
    return &f;
  }
  // Makes a function that returns the successor range of a node.  Returns a
  // pointer to that function.
  tgt::constant make_succ_func(
      compiler::ModuleCompiler const & compiler, int64_t arity
    )
  {
    tgt::function f = tgt::static_<tgt::function>(
        compiler.ir.rangefun_t, tgt::flexible(".succ")
      , {"node_p", "begin_out_pp", "end_out_pp"}
      );

    if(arity < 3)
    {
      tgt::scope _ = f;
      // Compute the size of pointers and get an integer type with the same
      // bitwidth.
      size_t sizeof_ptr = sizeof_(*compiler.ir.node_t);
      tgt::type size_t_ = tgt::types::int_(sizeof_ptr * 8);

      // Get the function paramters.
      tgt::value node_p = tgt::arg("node_p");
      tgt::ref begin_out_pp = tgt::arg("begin_out_pp");
      tgt::ref end_out_pp = tgt::arg("end_out_pp");

      // Compute the begining address.
      tgt::value begin_cp = &node_p.arrow(ND_SLOT0);
      tgt::value begin = begin_cp;
      tgt::value end = typecast(begin_cp, size_t_) + arity * sizeof_ptr;

      // Assign the outputs.
      begin_out_pp = begin;
      end_out_pp = end;
      tgt::return_();
    }
    else
      assert(0 && "arity > 2 not supported");

    return &f;
  }
}

namespace sprite { namespace compiler
{
  tgt::value vinvoke(tgt::value const & node_p, compiler::VtMember member)
    { return node_p.arrow(compiler::ND_VPTR).arrow(member)(node_p); }

  tgt::value vinvoke(
      tgt::value const & node_p, compiler::VtMember member, tgt::attribute attr
    )
  { return vinvoke(node_p, member).set_attribute(attr); }

  ModuleSTab::ModuleSTab(
      LibrarySTab & lib_stab_, curry::Module const & src
    )
    : source(&src), module_ir(src.name), nodes()
  {
    // Load the compiler data (e.g., headers) with the module in scope.
    scope _ = module_ir;
    this->compiler.reset(new ModuleCompiler(lib_stab_));
  }

  // Constructs an expression at the given node address.
  value construct(
      compiler::ModuleCompiler const & compiler
    , tgt::value const & root_p
    , curry::Rule const & expr
    )
  {
    ::Rewriter c(compiler, root_p);
    expr.visit(c);
    return c.target_p;
  }

  void prettyprint(curry::Library const & lib)
  {
    using std::cout;
    using std::endl;
    for(auto const & module: lib.modules)
    {
      cout << "Module: " << module.name << "\n";
      cout << "Imports:";
      for(auto const & import: module.imports)
        cout <<  " " << import;
      cout << "\n";
      cout << "DataTypes:";
      for(auto const & dtype: module.datatypes)
      {
        cout << "\n    " << dtype.name << " = ";
        bool first = true;
        for(auto const & ctor: dtype.constructors)
        {
          if(!first) { cout << " | "; } else { first = false; }
          cout << ctor.name << "(" << ctor.arity << ")";
        }
      }
      cout << "\nFunctions:";
      for(auto const & fun: module.functions)
      {
        cout << "\n    " << fun.name << "(" << fun.arity << ")";
      }
      cout << "\n\n";
    }
  }

  void compile(
      curry::Library const & lib
    , compiler::LibrarySTab & stab
    )
  {
    for(auto const & cymodule: lib.modules)
    {
      // Create a new LLVM module and symbol table entry.
      auto rv = stab.modules.emplace(
          cymodule.name, ModuleSTab{stab, cymodule}
        );
      compiler::ModuleSTab & module_stab = rv.first->second;

      // Helpful aliases.
      tgt::module & module_ir = module_stab.module_ir;
      compiler::ModuleCompiler const & compiler = *module_stab.compiler;
      
      // Set the module as the current scope.  Subsequent statements will add
      // functions, type definitions, and data to this module.
      tgt::scope _ = module_ir;
  
      // Create the null step function.
      function nullstep(module_ir->getFunction(".nullstep"));
      if(!nullstep.ptr())
        nullstep = inline_(compiler.ir.stepfun_t, ".nullstep", {}, []{});

      // Create the vtable for FWD nodes.
      tgt::function fwd_name = tgt::inline_<tgt::function>(
          compiler.ir.labelfun_t, ".fwd.name", {"node_p"}
        , [&]
          {
            // REFACTOR
            tgt::value node_p = arg("node_p");
            node_p = bitcast(node_p.arrow(ND_SLOT0), *compiler.ir.node_t);
            return_(
                node_p.arrow(ND_VPTR).arrow(VT_LABEL)(node_p)
                    .set_attribute(tailcall)
              );
          }
        );
      // FIXME
      tgt::function fwd_arity = tgt::inline_<tgt::function>(
          compiler.ir.arityfun_t, "fwd.arity", {"node_p"}
        , [&]{
            tgt::value node_p = arg("node_p");
            node_p = bitcast(node_p.arrow(ND_SLOT0), *compiler.ir.node_t);
            return_(
                node_p.arrow(ND_VPTR).arrow(VT_ARITY)(node_p)
                    .set_attribute(tailcall)
              );
          }
        );
      tgt::function fwd_succ = tgt::inline_<tgt::function>(
          compiler.ir.rangefun_t, "fwd.succ"
        , {"node_p", "begin_out_pp", "end_out_pp"}
        , [&]{
            tgt::value node_p = arg("node_p");
            node_p = bitcast(node_p.arrow(ND_SLOT0), *compiler.ir.node_t);
            node_p.arrow(ND_VPTR).arrow(VT_SUCC)(
                node_p, arg("begin_out_pp"), arg("end_out_pp")
              ).set_attribute(tailcall);
            return_();
          }
        );
      tgt::function fwd_N = inline_(
          compiler.ir.stepfun_t, ".fwd.N", {"node_p"}
        , [&]{
            tgt::value node_p = arg("node_p");
            node_p = bitcast(node_p.arrow(ND_SLOT0), *compiler.ir.node_t);
            vinvoke(node_p, VT_N, tailcall);
            return_();
          }
        );
      tgt::function fwd_H = inline_(
          compiler.ir.stepfun_t, ".fwd.H", {"node_p"}
        , [&]{
            tgt::value node_p = arg("node_p");
            node_p = bitcast(node_p.arrow(ND_SLOT0), *compiler.ir.node_t);
            vinvoke(node_p, VT_H, tailcall);
            return_();
          }
        );

      module_stab.vt_fwd_p.reset(
          static_(compiler.ir.vtable_t, ".fwd.vt")
              .set_initializer(_t(&fwd_name, &fwd_arity, &fwd_succ, &fwd_N, &fwd_H))
        );

      // Add the static vtable for each constructor to the module.
      for(auto const & dtype: cymodule.datatypes)
      {
        size_t tag = compiler::CTOR;
        for(auto const & ctor: dtype.constructors)
        {
          // Lookup the N function and define it as needed.
          std::string const n_name = ".N." + std::to_string(ctor.arity);
          function N(module_ir->getFunction(n_name.c_str()));
          if(!N.ptr())
          {
            N = inline_(
                compiler.ir.stepfun_t, n_name, {"root_p"}
              , [&]{
                  tgt::value root_p = arg("root_p");
                  tgt::value child;
                  // FIXME: need to implement "dot" and "arrow" with names.
                  switch(ctor.arity)
                  {
                    case 2:
                      child = bitcast(
                          root_p.arrow(ND_SLOT1), *compiler.ir.node_t
                        );
                      child.arrow(ND_VPTR).arrow(VT_N)(child);
                    case 1:
                      child = bitcast(
                          root_p.arrow(ND_SLOT0), *compiler.ir.node_t
                        );
                      child.arrow(ND_VPTR).arrow(VT_N)(child)
                          .set_attribute(tailcall);
                    case 0:
                      break;
                    default:
                    {
                      ref chldrn = bitcast(
                          root_p.arrow(ND_SLOT0), *compiler.ir.node_t
                        );
                      tgt::value last_call;
                      for(size_t i=0; i<ctor.arity; ++i)
                      {
                        child = chldrn[i];
                        last_call = child.arrow(ND_VPTR).arrow(VT_N)(child);
                      }
                      last_call.set_attribute(tailcall);
                      break;
                    }
                  }
                  tgt::return_();
                }
              );
          }
  
          // Create the vtable.
          tgt::globalvar vt = static_(
              compiler.ir.vtable_t, ".vt.CTOR." + ctor.name
            )
              .set_initializer(_t(
                  make_name_func(compiler, ctor.name)
                , make_arity_func(compiler, ctor.arity) // FIXME
                , make_succ_func(compiler, ctor.arity)
                , &N, &nullstep
                ));
  
          // Update the symbol tables.
          module_stab.nodes.emplace(
              ctor.name, compiler::NodeSTab(ctor, vt, tag++)
            );
        }
      }
  
      // Update the symbol tables with the functions.
      for(auto const & fun: cymodule.functions)
      {
        // Forward declaration.
        function step = inline_(
            compiler.ir.stepfun_t, ".step." + fun.name, {"root_p"}
          );
        function N = inline_(
            compiler.ir.stepfun_t, ".N." + fun.name, {"root_p"}
          , [&]{
              tgt::value root_p = arg("root_p");
              step(root_p);
              vinvoke(root_p, VT_N, tailcall);
              return_();
            }
          );
        function H = inline_(
            compiler.ir.stepfun_t, ".H." + fun.name, {"root_p"}
          , [&]{
              tgt::value root_p = arg("root_p");
              step(root_p);
              vinvoke(root_p, VT_H, tailcall);
            }
          );
  
        tgt::globalvar vt =
            static_(compiler.ir.vtable_t, ".vtable.for." + fun.name)
            .set_initializer(_t(
                make_name_func(compiler, fun.name)
              , make_arity_func(compiler, fun.arity) // FIXME
              , make_succ_func(compiler, fun.arity)
              , &N, &H
              ));

        module_stab.nodes.emplace(
            fun.name
          , compiler::NodeSTab(fun, vt, compiler::OPER)
          );
      }
  
      // Compile the functions.
      for(auto const & fun: cymodule.functions)
      {
        auto step = module_ir.getglobal(".step." + fun.name);
        tgt::scope _ = dyn_cast<function>(step);
        ::compile_function(compiler, arg("root_p"), fun);
      }
    }
  }
}}

