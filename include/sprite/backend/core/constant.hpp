#pragma once
#include "sprite/backend/config.hpp"
#include "sprite/backend/core/type.hpp"
#include "sprite/backend/core/value.hpp"
#include "llvm/IR/Constants.h"

namespace sprite { namespace backend
{
  template<typename T>
  struct constobj : valueobj<T>
  {
    using basic_type = Constant;
    using valueobj<T>::valueobj;

    /// Gets an element of a constant aggregate.
    constant operator[](size_t i) const
    {
      auto const ii = static_cast<unsigned>(i);
      return constant(SPRITE_APICALL((*this)->getAggregateElement(ii)));
    }

  private:

    static_assert(
        std::is_base_of<basic_type, T>::value, "Expected an LLVM Constant object"
      );
  };
}}