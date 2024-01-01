#include "fixed_point.hpp"

namespace zsl::pp::test
{

static_assert(pow(10, 9) == 1'000'000'000);
  
static_assert(fixed_decimal_t<>::min().rep() == -999'999'999'999'999'999LL);
static_assert(fixed_decimal_t<>::max().rep() == +999'999'999'999'999'999LL);
static_assert(fixed_decimal_t<>::from_raw(1234567890).rep() == 1234567890);
	    
}

