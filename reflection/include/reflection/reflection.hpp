#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <bit>

#include <types/string_literal.hpp>
#include <types/estring.hpp>

namespace zsl::reflection
{

//  Rationale:
//    Basic types should require no explanation
//
//    Fixed string -> UUID, ccy pair code etc.
//    Bounded string -> user names, timezone names etc.
//    Text -> documents, XML, JSON configuration etc.
//    Typealias -> similar to boost.strongypedef, Haskell/Rust newtype
//                 looks, quacks, walks, flies like a duck but cannot be converted to a duck
//    Pointer -> owning pointer to other types, i.e. std::unique_ptr
//    Optional -> optional fields, i.e. std::optional
//    Array -> fixed size list of anything except ARRAY/VECTOR
//    Vector -> variable size list of anything except ARRAY/VECTOR
enum reflected_fieldype_t
{
    UNDEFINED,
    BOOL,
    I8, I16, I32, I64, INTEGRAL,
    UI8, UI16, UI32, UI64,
    F32, F64, F128,
    FIXED_STRING, BOUNDED_STRING, TEXT,
    BINARY,
    ENUM,
    TYPEALIAS,
    OBJECT,
    POINTER,
    OPTIONAL,
    ARRAY, VECTOR,
};

template < typename T > inline constexpr reflected_fieldype_t reflected_fieldype_v = reflected_fieldype_t::UNDEFINED;

template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < bool > = reflected_fieldype_t::BOOL;

template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < int8_t > = reflected_fieldype_t::I8;
template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < int16_t > = reflected_fieldype_t::I16;
template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < int32_t > = reflected_fieldype_t::I32;
template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < int64_t > = reflected_fieldype_t::I64;

template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < uint8_t > = reflected_fieldype_t::UI8;
template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < uint16_t > = reflected_fieldype_t::UI16;
template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < uint32_t > = reflected_fieldype_t::UI32;
template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < uint64_t > = reflected_fieldype_t::UI64;

template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < float > = reflected_fieldype_t::F32;
template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < double > = reflected_fieldype_t::F64;
template <> inline constexpr reflected_fieldype_t reflected_fieldype_v < long double > = reflected_fieldype_t::F128;

static_assert(sizeof(long double) == 16);

using zsl::types::fixed_estring_t;
using zsl::types::bounded_estring_t;

template < uint8_t N > constexpr reflected_fieldype_t reflected_fieldype_v < fixed_estring_t < N > > = reflected_fieldype_t::FIXED_STRING;
template < uint8_t N > constexpr reflected_fieldype_t reflected_fieldype_v < bounded_estring_t < N > > = reflected_fieldype_t::BOUNDED_STRING;

template < typename T >
concept reflectable_class_t = std::is_class_v < T > && !std::is_polymorphic_v < T >;

using zsl::types::string_literal_t;

struct reflected_standin_class_t;
using reflected_standin_field_t = int32_t reflected_standin_class_t::*;

struct field_reflector_t
{
    template < typename T, typename U, typename F = U T::* >
    constexpr field_reflector_t(string_literal_t const & name, U (T::*f))
    : name_{name}
    , ptr_{std::bit_cast < int32_t reflected_standin_class_t::* >(f)}
    , type_{reflected_fieldype_v < U >}
    {
    }

    string_literal_t name_;
    reflected_standin_field_t ptr_{};
    reflected_fieldype_t type_{};
};

struct class_reflector_t
{
    string_literal_t name_;
    std::initializer_list < field_reflector_t > fields_;
};

template < typename T >
void register_reflected_class(class_reflector_t const & /* cr */)
{
}

template < reflectable_class_t T >
constexpr auto reflect(string_literal_t const & name, std::initializer_list < field_reflector_t > frs)
{
    register_reflected_class < T >({
        .name_ = name,
        .fields_ = frs
    });
}

}
