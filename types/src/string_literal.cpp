module;

#include <cstdint>
#include <string_view>

export module zsl.types.string_literal;

export namespace zsl::types
{

template < typename Char >
struct basic_string_literal_t : private std::basic_string_view < Char >
{
    using impl_t = std::basic_string_view < Char >;

    template < size_t N >
    consteval basic_string_literal_t(Char const (&s)[N]) : impl_t{s}
    {
    }

    consteval auto c_str() const
    {
        return data();
    }

    using impl_t::begin;
    using impl_t::end;
    using impl_t::cbegin;
    using impl_t::cend;
    using impl_t::length;
    using impl_t::data;
};

using string_literal_t = basic_string_literal_t < char >;

}
