#pragma once

#include <cstdint>
#include <algorithm>
#include <array>
#include <ranges>
#include <stdexcept>

namespace zsl::types
{

//  length excludes null terminator
template < uint8_t MIN_LENGTH, uint8_t MAX_LENGTH >
struct estring_t
{
    constexpr estring_t(std::string_view const s) : v_{init(s)}
    {
    }

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
        return v_[MAX_LENGTH];
    }

    constexpr auto length() const
    {
        return MAX_LENGTH - remaining_capacity();
    }

    constexpr auto size() const
    {
        return length();
    }

    constexpr auto c_str() const
    {
        return &v_[0];
    }

private:
    static_assert(MIN_LENGTH <= MAX_LENGTH, "estring_t: min length is greater than max length");
    static_assert(MIN_LENGTH >= 0, "estring_t: min non-null terminated length is 0");
    static_assert(MAX_LENGTH < 64, "estring_t: max non-null terminated length is 63");
    constexpr static auto const CAPACITY = MAX_LENGTH + 1;
    using storage_t = std::array < char, CAPACITY >;
    alignas(sizeof(int64_t)) storage_t v_{init()};

    consteval static auto init()
    {
        storage_t v{};
        v[MAX_LENGTH] = MAX_LENGTH;
        return v;
    }

    constexpr static auto init(std::string_view const s)
    {
        if (s.length() < MIN_LENGTH)
            throw std::runtime_error("estring_t::init: initializing with string shorter than MIN_LENGTH");
        if (s.length() > MAX_LENGTH)
            throw std::runtime_error("estring_t::init: initializing with string longer than MAX_LENGTH");
        storage_t v{};
        std::ranges::copy(s, v.begin());
        v[s.length()] = '\0';
        v[MAX_LENGTH] = MAX_LENGTH - s.length();
        return v;
    }
};

template < uint8_t N >
using fixed_estring_t = estring_t < N, N >;

template < uint8_t MAX_LENGTH >
using bounded_estring_t = estring_t < 0, MAX_LENGTH >;

template < typename tagType, uint8_t MIN_LENGTH, uint8_t MAX_LENGTH >
struct typed_estring_t : estring_t < MIN_LENGTH, MAX_LENGTH >
{
    using impl_t = estring_t < MIN_LENGTH, MAX_LENGTH >;
    using impl_t::impl_t;
};

template < typename tagType, uint8_t N >
using typed_fixed_estring_t = typed_estring_t < tagType, N, N >;

template < typename tagType, uint8_t MAX_LENGTH >
using typed_bounded_estring_t = typed_estring_t < tagType, 0, MAX_LENGTH >;

}
