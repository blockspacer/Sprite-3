#pragma once
#include "sprite/backend/core/fwd.hpp"
#include "sprite/backend/support/special_values.hpp"
#include <type_traits>

#define SPRITE_DEF_ISOBJ_CHECK(name)                                    \
    template<typename T> struct is_##name : std::false_type {};         \
    template<typename T> struct is_##name<name<T>> : std::true_type {}; \
  /**/

// Multi-dimensional initializer lists.
#define SPRITE_INIT_LIST1(T) std::initializer_list<T>
#define SPRITE_INIT_LIST2(T) std::initializer_list<SPRITE_INIT_LIST1(T)>
#define SPRITE_INIT_LIST3(T) std::initializer_list<SPRITE_INIT_LIST2(T)>
#define SPRITE_INIT_LIST4(T) std::initializer_list<SPRITE_INIT_LIST3(T)>
#define SPRITE_INIT_LIST5(T) std::initializer_list<SPRITE_INIT_LIST4(T)>
#define SPRITE_INIT_LIST6(T) std::initializer_list<SPRITE_INIT_LIST5(T)>
#define SPRITE_INIT_LIST7(T) std::initializer_list<SPRITE_INIT_LIST6(T)>
#define SPRITE_INIT_LIST8(T) std::initializer_list<SPRITE_INIT_LIST7(T)>
#define SPRITE_INIT_LIST9(T) std::initializer_list<SPRITE_INIT_LIST8(T)>

// These macros simplify the look of certain declarations in the API.
#define SPRITE_ENABLE_FOR_FUNCTION_PROTOTYPES(Args)                                      \
    typename = typename std::enable_if<alltt<is_typearg_or_ellipsis, Args>::value>::type \
  /**/

#define SPRITE_ENABLE_FOR_CONSTANT_INITIALIZERS(Args)                                     \
    typename = typename std::enable_if<alltt<is_constant_initializer, Args>::value>::type \
  /**/

#define SPRITE_ENABLE_FOR_VALUES(Args)                                        \
    typename = typename std::enable_if<alltt<is_valuearg, Args>::value>::type \
  /**/

#define SPRITE_DISABLE_FOR_VALUES(Args)                                        \
    typename = typename std::enable_if<!alltt<is_valuearg, Args>::value>::type \
  /**/

#define SPRITE_ENABLE_FOR_VALUE_INITIALIZERS(Args)                                     \
    typename = typename std::enable_if<alltt<is_value_initializer, Args>::value>::type \
  /**/

namespace sprite { namespace backend
{
  SPRITE_DEF_ISOBJ_CHECK(object)    // is_object
  SPRITE_DEF_ISOBJ_CHECK(typeobj)   // is_typeobj
  SPRITE_DEF_ISOBJ_CHECK(valueobj)  // is_valueobj
  SPRITE_DEF_ISOBJ_CHECK(constobj)  // is_constobj
  SPRITE_DEF_ISOBJ_CHECK(globalobj) // is_globalobj
  SPRITE_DEF_ISOBJ_CHECK(instrobj)  // is_instrobj

  /**
   * @brief Creates a dummy instance of T for the compiler.  Cannot be called,
   * but can be used with decltype.
   */

  template<typename T> T const & instanceof();

  //@{
  /// "all" for a type trait.
  template<template<typename> class Trait, typename...Ts>
  struct alltt;

  template<template<typename> class Trait, typename T, typename...Ts>
  struct alltt<Trait, T, Ts...>
    : std::integral_constant<bool
        , Trait<T>() && alltt<Trait, Ts...>::value
        >
  {};

  template<template<typename> class Trait>
  struct alltt<Trait>
    : std::true_type
  {};
  //@}

  //@{
  /// "any" for a type trait.
  template<template<typename> class Trait, typename...Ts>
  struct anytt;

  template<template<typename> class Trait, typename T, typename...Ts>
  struct anytt<Trait, T, Ts...>
    : std::integral_constant<bool
        , Trait<T>() || anytt<Trait, Ts...>::value
        >
  {};

  template<template<typename> class Trait>
  struct anytt<Trait>
    : std::true_type
  {};
  //@}

  /**
   * @brief True if a value of type @p T can be accepted as an LLVM object of
   * type @p What*.
   *
   */
  template<typename T, typename LlvmApiType>
  struct is_api_arg
    : std::is_constructible<
        LlvmApiType*, decltype(ptr(instanceof<T>()))
      >
  {};

  /**
   * @brief True if a value of type @p T can be accepted as an LLVM type value.
   *
   * If @p SpecificType is supplied, then the the LLVM type must be that type
   * (or a subtype of it).
   */
  template<typename T> using is_typearg = is_api_arg<T, Type>;

  /**
   * @brief True if a value of type @p T can be accepted as an LLVM constant
   * value.
   *
   * If @p SpecificType is supplied, then the the LLVM constant type must be
   * that type (or a subtype of it).
   */
  template<typename T> using is_constarg = is_api_arg<T, Constant>;

  /**
   * @brief True if a value of type @p T can be accepted as an LLVM Value.
   */
  template<typename T> using is_valuearg = is_api_arg<T, Value>;

  /// Identifies legal arguments for forming function signatures.
  template<typename T>
  struct is_typearg_or_ellipsis
    : std::integral_constant<bool
        , is_typearg<T>::value || std::is_convertible<T, ellipsis>::value
        >
  {};

  /**
   * @brief Identifies legal arguments for initializing constants.
   *
   * A raw initializer is defined as "not a typearg, ellipsis, or valuearg."
   */
  template<typename T>
  struct is_constant_initializer
    : std::integral_constant<
          bool
        , is_constarg<T>::value
            || !(is_typearg_or_ellipsis<T>::value || is_valuearg<T>::value)
        >
  {};

  /**
   * @brief Identifies legal arguments for initializing values.
   */
  template<typename T>
  struct is_value_initializer
    : std::integral_constant<
          bool
        , is_valuearg<T>::value || is_constant_initializer<T>::value
        >
  {};

  //@{
  /**
   * @brief The nested type specifier @p type is @p U, if @p T is derived from
   * <tt>object<U></tt> for some type @p U, else @p T.
   */
  template<typename T> struct remove_object_wrapper { using type = T; };

  template<template<typename> class Obj, typename U>
  struct remove_object_wrapper<Obj<U>>
  {
    using type = typename std::conditional<
        std::is_base_of<object<U>, Obj<U>>::value, U, Obj<U>
      >::type;
  };
  //@}

  //@{
  /// Builds a <tt>std::initializer_list<T></tt> nested to depth @p N.
  template<typename T, size_t N=0> struct nested_initializer_list
  {
  private:
    using next_type = typename nested_initializer_list<T, N-1>::type;
  public:
    using type = std::initializer_list<next_type>;
  };

  template<typename T> struct nested_initializer_list<T, 0>
    { using type = T; };
  //@}

  /**
   * @brief Produces a type capable of accepting a (possibly nested) @p
   * initializer_list suitable for initializing array type @p T.
   */
  template<
      typename T
    , typename ElementType = typename std::remove_all_extents<T>::type
    >
  struct nested_initializer_list_for
  {
    using type = typename nested_initializer_list<
        ElementType, std::rank<T>::value
      >::type;
  };

  /**
   * @brief Used to enable a function like <tt>get<Tgt></tt>, where the source
   * type is <tt>Src const &</tt>.
   *
   * Attempts to avoid extra copies.  Indicates whether a request for return
   * type @p Tgt can be satisfied by returning <tt>Src const &</tt>.
   */
  template<typename Tgt, typename Src>
  class can_get_as_cref
  {
    using Tgt_ = typename std::remove_const<
        typename std::remove_reference<Tgt>::type
      >::type;

  public:

    static bool constexpr value = std::is_same<Tgt_, Src>::value;
  };

  /// True if @p T is a 1/2/4/8-byte integer or float/double.
  template<typename T> struct is_simple_data_type
    : std::integral_constant<
          bool
        , std::is_same<T, char>::value // Note char != int8_t != uint8_t
            || std::is_same<T, wchar_t>::value
            || std::is_same<T, int8_t>::value
            || std::is_same<T, int16_t>::value
            || std::is_same<T, int32_t>::value
            || std::is_same<T, int64_t>::value
            || std::is_same<T, uint8_t>::value
            || std::is_same<T, uint16_t>::value
            || std::is_same<T, uint32_t>::value
            || std::is_same<T, uint64_t>::value
            || std::is_same<T, float>::value
            || std::is_same<T, double>::value
        >
  {};

  /// Indicates whether @p T is a simple array type.
  template<typename T> struct is_simple_data_array
    : std::conditional<
          std::is_array<T>::value
        , typename is_simple_data_type<
              typename std::remove_all_extents<T>::type
            >::type
        , std::false_type
        >::type
  {};

  /// True if @p T is an array, but not a simple data array.
  template<typename T> struct is_nonsimple_array
    : std::integral_constant<
          bool, std::is_array<T>::value && !is_simple_data_array<T>::value
        >
  {};

  //@{
  /// True if @p T is a std::tuple.
  template<typename T> struct is_tuple
    : std::false_type
  {};

  template<typename...T> struct is_tuple<std::tuple<T...>>
    : std::true_type
  {};
  //@}
}}

#undef SPRITE_DEF_ISOBJ_CHECK
