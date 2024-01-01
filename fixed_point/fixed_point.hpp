#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <string>

namespace zsl::pp
{

struct fixed_decimal_base_t
{
protected:
  template < typename T >
  constexpr T static power(T const v, uint8_t e)
  {
    T r = 1;
    while (e--)
      r *= v;
    return r;
  }
  
};
  
template < uint8_t integral_digits_ = 9 /* 30 bits */, uint8_t fractional_digits_ = 9 /* 30 bits */ >
struct fixed_decimal_t : fixed_decimal_base_t
{
private:
  using self_t = fixed_decimal_t;
  using rep_t = int64_t;
  using result_t = __int128_t;
  
  rep_t v_{};

  constexpr fixed_decimal_t(rep_t const v) : v_{v}
  {
  }

public:
  constexpr fixed_decimal_t() : v_{}
  {
  }
  
  constexpr static auto integral_digits()
  {
    return integral_digits_;
  }

  constexpr static auto fractional_digits()
  {
    return fractional_digits_;
  }

  constexpr static auto total_digits()
  {
    return integral_digits() + fractional_digits();
  }

  constexpr static auto scale_factor()
  {
    return power(10LL, fractional_digits());
  }
  
  constexpr static auto from_raw_unsafe(rep_t const v)
  {
    return fixed_decimal_t(v);
  }

  constexpr throw_out_of_range()
  {
    throw std::out_of_range("fixed_decimal value out of range" + std::to_string(v));
  }

  constexpr static bool in_range_raw(rep_t const v)
  {
    return v <= rep_max() && v >= rep_min();
  }
  
  constexpr static auto from_raw(rep_t const v)
  {
    if (in_range_raw(v))
      return from_raw_unsafe(repv);
    else
      throw_out_of_range("fixed_decimal value out of range" + std::to_string(v));
  }
  
  constexpr static auto from(long double const v)
  {
    if (auto const repv = std::llround(v * scale_factor()); in_range_raw(repv))
      return from_raw(repv);
    else
      throw_out_of_range("fixed_decimal value out of range" + std::to_string(v));
  }

  constexpr static auto from(rep_t const v)
  {
    if (auto const repv = std::llround(v * scale_factor()); in_range_raw(repv))
      return from_raw(repv);
    else
      throw_out_of_range("fixed_decimal value out of range" + std::to_string(v));
  }

  constexpr static rep_t rep_max()
  {
    return power(10LL, total_digits()) - 1;
  }

  constexpr static rep_t rep_min()
  {
    return -rep_max();
  }

  constexpr static long double float_max()
  {
    return rep_max();
  }

  constexpr static long double float_min()
  {
    return rep_min();
  }

  constexpr static rep_t int_max()
  {
    return power(10, integral_digits()) - 1;
  }

  constexpr static rep_t int_min()
  {
    return -int_max();
  }

  constexpr static self_t max() noexcept
  {
    return from_raw(rep_max());
  }

  constexpr static self_t min() noexcept
  {
    return from_raw(rep_min());
  }
  
  constexpr auto rep() const
  {
    return v_;
  }

  constexpr explicit operator double () const
  {
    return rep() / scale_factor();
  }
  
  constexpr friend auto operator - (self_t const & rhs)
  {
    return from_raw(-rhs.rep());
  }

  constexpr friend auto operator + (self_t const & lhs, self_t const & rhs)
  {
    return from_raw(lhs.rep() + rhs.rep());
  }

  constexpr friend auto operator - (self_t const & lhs, self_t const & rhs)
  {
    return from_raw(lhs.rep() - rhs.rep());
  }

  constexpr friend auto operator * (self_t const & lhs, self_t const & rhs)
  {
    //  naive...
    if (result_t const r = lhs.rep() * rhs.rep(); r <= rep_max())
      return from_raw(lhs.rep() / scale_factor() * rhs.rep());
  }
};

}

