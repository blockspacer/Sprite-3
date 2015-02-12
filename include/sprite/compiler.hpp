#pragma once
#include "sprite/backend.hpp"
#include "sprite/backend/support/testing.hpp"
#include "sprite/curryinput.hpp"
#include "sprite/runtime.hpp"
#include "sprite/basic_runtime.hpp"
#include <unordered_map>
#include <iterator>

namespace sprite { namespace compiler
{
  using namespace sprite::backend;

  // ===========================
  // ====== Symbol tables ======
  // ===========================

  /**
   * @brief Node symbol table.
   */
  struct NodeSTab
  {
    NodeSTab(
        curry::Constructor const & source_
      , sprite::backend::globalvar const & vtable_
      , tag_t tag_
      )
      : source(&source_), vtable(vtable_), tag(tag_)
    {}

    NodeSTab(
        curry::Function const & source_
      , sprite::backend::globalvar const & vtable_
      , tag_t tag_
      )
      : source(reinterpret_cast<curry::Constructor const *>(&source_))
      , vtable(vtable_), tag(tag_)
    {}

    // Disabled because globalvar is a reference.
    NodeSTab & operator=(NodeSTab const &) = delete;

    // Information about the source declaration for this node.  Note: this may
    // be a Constructor or Function.  The structs are layout-compatible in the
    // head, so using a reinterpret_cast may be safe (depending on the value of
    // the tag).
    curry::Constructor const * source;

    // The vtable for this node, in LLVM IR.
    sprite::backend::globalvar vtable;

    // The tag associated with the node.  Note: tag==OPER indicates a function;
    // other tag values (i.e., tag>=CTOR) indicate constructors.
    tag_t tag;
  };

  struct LibrarySTab;
  struct ModuleCompiler;

  /// Module symbol table.
  struct ModuleSTab
  {
    // Initialize while creating a new LLVM module for holding the IR.
    ModuleSTab(curry::Module const &, llvm::LLVMContext &);

    // Initialize from an existing LLVM module.
    ModuleSTab(curry::Module const &, sprite::backend::module const & M);

    // Disabled because globalvar is a reference (see NodeSTab).
    ModuleSTab & operator=(ModuleSTab const &) = delete;
    
    // Information about the source declaration for this module.
    curry::Module const * source;

    // The module, in LLVM IR.
    sprite::backend::module module_ir;

    // The node information.
    std::unordered_map<curry::Qname, NodeSTab> nodes;

    // Look up a node symbol table.
    compiler::NodeSTab const & lookup(curry::Qname const &) const;

    // The Sprite runtime library, in the target program.
    compiler::rt_h const & rt() const { return headers->rt; }

  private:

    // Headers must be loaded while the LLVM module for the Curry module this
    // class represents is the active one.
    struct Headers { compiler::rt_h rt; };
    std::shared_ptr<Headers> headers;
  };

  struct LibrarySTab
  {
    std::unordered_map<std::string, ModuleSTab> modules;

    /// Move the contents of another LibrarySTab into this one.
    LibrarySTab & merge_from(LibrarySTab & arg)
    {
      if(modules.empty())
        modules = std::move(arg.modules);
      else
      {
        for(auto && item: arg.modules)
          modules.emplace(std::move(item));
        arg.modules.clear();
      }
      return *this;
    }
  };

  // ==============================
  // ====== Module functions ======
  // ==============================

  /// Prints a module.
  void prettyprint(sprite::curry::Library const & lib);

  /**
   * @brief Updates the symbol table with the result of compiling the given
   * Curry module.
   *
   * If the symbol table already contains IR for any given module (e.g.,
   * because it was previously loaded from a .bc file), then the symbol tables
   * are updated without actually compiling any code.
   */
  void compile(
      sprite::curry::Module const &
    , sprite::compiler::LibrarySTab &
    , llvm::LLVMContext &
    , bool enable_tracing
    );

  /// Constructs an expression at the given node address.
  // FIXME: document parameters.
  // Returns the root of the expression as a node_t*.
  value construct(
      ModuleSTab const & module_stab
    , value const & root_p
    , sprite::curry::Rule const & expr
    );

  //@{
  /// Calls the virtual function of the given node.
  value vinvoke(value const & root_p, member_labels::VtMember mem);
  value vinvoke(
      value const & root_p, member_labels::VtMember mem, attribute attr
    );
  //@}

  /// Gets the vtable pointer from a node pointer.  Skips FWD nodes.
  value get_vtable(value node_p);

  /// Initializes a node.
  inline void node_init(
      value node_p
    , ModuleSTab const & module_stab
    , compiler::NodeSTab const & node_stab
    )
  {
    using namespace member_labels;
    node_p.arrow(ND_VPTR) = bitcast(
        &node_stab.vtable, *module_stab.rt().vtable_t
      );
    node_p.arrow(ND_TAG) = node_stab.tag;

    // For choices, assign a fresh id.
    if(node_stab.tag == CHOICE)
      node_p.arrow(ND_AUX) = module_stab.rt().next_choice_id++;
  }
}}

