#pragma once

#include <cstdint>
#include <algorithm>
#include <array>
#include <ranges>
#include <stdexcept>

namespace zsl::types
{

//  length excludes null terminator
template < uint8_t MIN_LEN, uint8_t MAX_LEN >
struct estring_t
{
    constexpr estring_t(std::string_view const s) : v_{init(s)}
    {
    }

    consteval std::size_t min_length() const
    {
        return MIN_LEN;
    }

    consteval std::size_t max_length() const
    {
        return MAX_LEN;
    }
    
    constexpr std::size_t remaining_capacity() const
    {
        return v_[MAX_LEN];
    }

    constexpr std::size_t length() const
    {
        return MAX_LEN - remaining_capacity();
    }

    constexpr auto size() const
    {
        return length();
    }

    constexpr auto c_str() const
    {
        return &v_[0];
    }

    constexpr explicit operator std::string_view () const &
    {
        return {c_str(), length()};
    }

    constexpr explicit operator std::string_view () && = delete;

    constexpr auto repr() const
    {
        if constexpr (MAX_LEN < 4)
            return std::byteswap(std::bit_cast < int32_t >(v_));
        else
        if constexpr (MAX_LEN < 8)
            return std::byteswap(std::bit_cast < int64_t >(v_));
        else
        if constexpr (MAX_LEN < 16)
            return std::byteswap(std::bit_cast < __int128_t >(v_));
        else
            return std::string_view(*this);
    }

    constexpr static auto storage_size()
    {
        return STORAGE_SIZE;
    }

protected:
    ~estring_t() = default;  //  never use this type directly

private:
    static_assert(MIN_LEN <= MAX_LEN, "min length is greater than max length");
    static_assert(MIN_LEN >= 0, "min non-null terminated length is 0");
    static_assert(MAX_LEN < 64, "max non-null terminated length is 63");
    constexpr static auto const STORAGE_SIZE = MAX_LEN < 4 ? 4 : (MAX_LEN < 8 ? 8 : (MAX_LEN < 16 ? 16 : (MAX_LEN + 8) / 8 * 8));
    using storage_t = std::array < char, STORAGE_SIZE >;
    alignas(sizeof(int64_t)) storage_t v_{init()};

    consteval static auto init()
    {
        storage_t v{};
        v[MAX_LEN] = MAX_LEN;
        return v;
    }

    constexpr static auto init(std::string_view const s)
    {
        if (s.length() < MIN_LEN)
            throw std::runtime_error("initializing with string shorter than MIN_LEN");
        if (s.length() > MAX_LEN)
            throw std::runtime_error("initializing with string longer than MAX_LEN");
        
        storage_t v{};
        std::copy(s.begin(), s.end(), v.begin());
        v[s.length()] = '\0';
        v[MAX_LEN] = MAX_LEN - s.length();
        return v;
    }
};

template < uint8_t MIN_LEN_1, uint8_t MAX_LEN_1, uint8_t MIN_LEN_2, uint8_t MAX_LEN_2 >
constexpr auto operator <=> (estring_t < MIN_LEN_1, MAX_LEN_1 > const & lhs, estring_t < MIN_LEN_2, MAX_LEN_2 > const & rhs)
{
    if constexpr (MAX_LEN_1 >= 16 || MAX_LEN_2 >= 16 || MAX_LEN_1 != MAX_LEN_2)
        return std::string_view{lhs} <=> std::string_view{rhs};
    else
        return lhs.repr() <=> rhs.repr();
}

template < uint8_t N >
struct fixed_estring_t : estring_t < N, N >
{
    using impl_t = estring_t < N, N >;
    using impl_t::impl_t;
};

template < uint8_t MAX_LEN >
struct bounded_estring_t : estring_t < 1, MAX_LEN >
{
    using impl_t = estring_t < 1, MAX_LEN >;
    using impl_t::impl_t;
};

template < typename tagType, uint8_t MIN_LEN, uint8_t MAX_LEN >
struct typed_estring_t : estring_t < MIN_LEN, MAX_LEN >
{
    using impl_t = estring_t < MIN_LEN, MAX_LEN >;
    using impl_t::impl_t;
};

template < typename tagType, uint8_t N >
using typed_fixed_estring_t = typed_estring_t < tagType, N, N >;

template < typename tagType, uint8_t MAX_LEN >
using typed_bounded_estring_t = typed_estring_t < tagType, 1, MAX_LEN >;

}
