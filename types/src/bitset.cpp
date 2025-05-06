module;

#include <array>
#include <bit>
#include <ranges>

export module zsl.types.bitset;

export namespace zsl::types
{

template < size_t N >
requires (std::popcount(N) == 1)
struct bitset
{
public:
    constexpr size_t ffs() const
    {
        if (auto const i = find([] (uint64_t const v) { return v != 0ULL; } ); i != bits_.end())
            return __builtin_ffsll(*i) + std::distance(bits_.begin(), i);
    }

private:
    using bits = std::array < uint64_t, N >;
    bits bits_{};

    template < typename F >
    constexpr auto find(F && f)
    {
        return std::ranges::find_if(bits_, std::forward < F >(f));
    }

    template < typename F >
    constexpr auto rfind(F && f)
    {
        return std::ranges::find_if(bits_ | std::views::reverse, std::forward < F >(f));
    }
};

}