#pragma once
#include "sprite/backend/core/scope.hpp"
#include "sprite/backend/core/value.hpp"
#include "llvm/IR/Metadata.h"

namespace sprite { namespace backend
{
  namespace aux
  {
    template<typename T>
    typename std::enable_if<
        !std::is_convertible<T, string_ref>::value, Value *
      >::type
    get_md_value(T && arg)
      { return get_value(std::forward<T>(arg)).ptr(); }

    template<typename T>
    typename std::enable_if<
        std::is_convertible<T, string_ref>::value, Value *
      >::type
    get_md_value(T && arg)
      { return llvm::MDString::get(scope::current_context(), arg); }
  }

  // API: metadata
  template<>
  struct valueobj<llvm::MDNode> : object<llvm::MDNode>
  {
    using basic_type = MDNode;
    using object<llvm::MDNode>::object;

    template<typename...Args>
    valueobj<llvm::MDNode>(Args&&...args) : object<llvm::MDNode>(nullptr)
    {
      Value * elts[sizeof...(args)]
          {aux::get_md_value(std::forward<Args>(args))...};
      this->px = llvm::MDNode::get(
          scope::current_context()
        , array_ref<Value*>(elts) // works if elts has zero elements
        );
    }

    // auto a = metadata("a tree");
    // auto b = metadata("int", a);
    // auto c = metadata("const int", b, 1);

    friend size_t len(metadata const & md)
      { return md->getNumOperands(); }

    value operator[](unsigned i)
      { return value((*this)->getOperand(i)); }


  private:

    static_assert(
        std::is_base_of<basic_type, llvm::MDNode>::value
      , "Expected an LLVM MDNode object"
      );
  };

  instruction & valueobj<llvm::Instruction>::set_metadata(string_ref kind)
    { return this->set_metadata(kind, metadata()); }

  instruction & valueobj<llvm::Instruction>::set_metadata(
      string_ref kind, metadata const & arg
    )
  {
    (*this)->setMetadata(kind, arg.ptr());
    return *this;
  }

  inline metadata
  valueobj<llvm::Instruction>::get_metadata(string_ref kind)
    { return metadata((*this)->getMetadata(kind)); }
}}
