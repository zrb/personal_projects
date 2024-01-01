#pragma once

#include <cstdint>
#include <type_traits>
#include <bit>

namespace zrb::reflection
{

template < uint8_t MIN_LENGTH, uint8_t MAX_LENGTH >
struct string_t
{
    consteval auto min_length() const
    {
        return MIN_LENGTH;
    }

    consteval auto max_length() const
    {
        return MAX_LENGTH;
    }
    
    constexpr auto remaining_capacity() const
    {
        return v_[CAPACITY];
    }

    constexpr auto length() const
    {
        return MAX_LENGTH - remaining_capacity();
    }

    constexpr auto size() const
    {
        return length();
    }

private:
    static_assert(MIN_LENGTH > 0, "string_t: min non-null terminated length is 1");
    static_assert(MAX_LENGTH < 64, "string_t: max non-null terminated length is 63");
    constexpr static auto const CAPACITY = MAX_LENGTH + 1;
    alignas(sizeof(int64_t)) char v_[CAPACITY] = { static_cast < char >(MAX_LENGTH) };
};

template < uint8_t N >
struct fixed_string_t : string_t < N, N >
{
};

template < uint8_t N >
struct bounded_string_t : string_t < 1, N >
{
};

template < typename tagType, uint8_t N >
struct typed_bounded_string_t : bounded_string_t < N >
{
};

template < typename tagType, uint8_t N >
struct typed_fixed_string_t : fixed_string_t < N >
{
};

//  Rationale:
//    Basic types should require no explanation
//
//    Fixed string -> UUID, ccy pair code etc.
//    Bounded string -> user names, timezone names etc.
//    Text -> documents, XML, JSON configuration etc.
//    Typealias -> similar to boost.strong_typedef, Haskell/Rust newtype
//                 looks, quacks, walks, flies like a duck but cannot be converted to a duck
//    Pointer -> owning pointer to other types, i.e. std::unique_ptr
//    Optional -> optional fields, i.e. std::optional
//    Array -> fixed size list of anything except ARRAY/VECTOR
//    Vector -> variable size list of anything except ARRAY/VECTOR
enum reflected_field_types_t
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

template < typename T > inline constexpr reflected_field_types_t reflected_field_type_v = reflected_field_types_t::UNDEFINED;

template <> inline constexpr reflected_field_types_t reflected_field_type_v < bool > = reflected_field_types_t::BOOL;

template <> inline constexpr reflected_field_types_t reflected_field_type_v < int8_t > = reflected_field_types_t::I8;
template <> inline constexpr reflected_field_types_t reflected_field_type_v < int16_t > = reflected_field_types_t::I16;
template <> inline constexpr reflected_field_types_t reflected_field_type_v < int32_t > = reflected_field_types_t::I32;
template <> inline constexpr reflected_field_types_t reflected_field_type_v < int64_t > = reflected_field_types_t::I64;

template <> inline constexpr reflected_field_types_t reflected_field_type_v < uint8_t > = reflected_field_types_t::UI8;
template <> inline constexpr reflected_field_types_t reflected_field_type_v < uint16_t > = reflected_field_types_t::UI16;
template <> inline constexpr reflected_field_types_t reflected_field_type_v < uint32_t > = reflected_field_types_t::UI32;
template <> inline constexpr reflected_field_types_t reflected_field_type_v < uint64_t > = reflected_field_types_t::UI64;

template <> inline constexpr reflected_field_types_t reflected_field_type_v < float > = reflected_field_types_t::F32;
template <> inline constexpr reflected_field_types_t reflected_field_type_v < double > = reflected_field_types_t::F64;
template <> inline constexpr reflected_field_types_t reflected_field_type_v < long double > = reflected_field_types_t::F128;

template < uint8_t N > constexpr reflected_field_types_t reflected_field_type_v < fixed_string_t < N > > = reflected_field_types_t::FIXED_STRING;
template < uint8_t N > constexpr reflected_field_types_t reflected_field_type_v < bounded_string_t < N > > = reflected_field_types_t::BOUNDED_STRING;

template < typename T >
concept reflected_class_t = std::is_class_v < T > && !std::is_polymorphic_v < T >;

template < typename T, typename U >
struct reflected_field_t
{
    using ptr_t = U T::*;
    char const * const name_{};
    ptr_t ptr_{};
};

struct reflection_standin_class_t;

template < reflected_class_t T, typename... U >
constexpr auto reflect(char const * name, reflected_field_t < T, U > &&... field)
{
    register_reflected_class({
        .name_ = name,
        .constructor = +[] () { return T{}; },
        .fields = {{
            .name = field.name_,
            .ptr = std::bit_cast < int32_t reflection_standin_class_t::* >(field.ptr_),
        }...}
    });
}

}
