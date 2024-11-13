#include <catch2/catch_all.hpp>

#include <types/estring.hpp>

#include <iostream>

namespace zsl::types::tests
{

struct type_tag_CcyCode;
using CcyCode = typed_fixed_estring_t < type_tag_CcyCode, 3 >;
static_assert(CcyCode{"EUR"}.length() == 3);
static_assert((CcyCode{"EUR"} <=> CcyCode{"EUR"}) == std::strong_ordering::equal);
static_assert((CcyCode{"EUR"} <=> CcyCode{"GBP"}) == std::strong_ordering::less);
static_assert((CcyCode{"EUR"} <=> CcyCode{"AUD"}) == std::strong_ordering::greater);

struct type_tag_CcyPairCode;
using CcyPairCode = typed_fixed_estring_t < type_tag_CcyPairCode, 6 >;
static_assert(CcyPairCode{"EURUSD"}.length() == 6);
static_assert((CcyPairCode{"EURUSD"} <=> CcyPairCode{"EURUSD"}) == std::strong_ordering::equal);
static_assert((CcyPairCode{"EURUSD"} <=> CcyPairCode{"GBPUSD"}) == std::strong_ordering::less);
static_assert((CcyPairCode{"EURUSD"} <=> CcyPairCode{"AUDUSD"}) == std::strong_ordering::greater);

static_assert((CcyCode{"EUR"} <=> CcyPairCode{"EURUSD"}) == std::strong_ordering::less);
static_assert((CcyCode{"EUR"} <=> CcyPairCode{"AUDUSD"}) == std::strong_ordering::greater);
static_assert((CcyPairCode{"EURUSD"} <=> CcyCode{"GBP"}) == std::strong_ordering::less);
static_assert((CcyPairCode{"EURUSD"} <=> CcyCode{"EUR"}) == std::strong_ordering::greater);

static_assert(bounded_estring_t < 8 >{"hello"}.length() == 5);
static_assert((bounded_estring_t < 8 >{"hello"} <=> bounded_estring_t < 8 >{"hello..."}) == std::strong_ordering::less);

struct type_tag_TenorCode;
using TenorCode = typed_bounded_estring_t < type_tag_TenorCode, 12 >;

struct type_tag_UserID;
using UserID = typed_bounded_estring_t < type_tag_UserID, 20 >;

struct type_tag_UserName;
using UserName = typed_bounded_estring_t < type_tag_UserName, 24 >;

static_assert(CcyCode::storage_size() == 4);
static_assert(CcyPairCode::storage_size() == 8);
static_assert(TenorCode::storage_size() == 16);
static_assert(UserID::storage_size() == 24);
static_assert(UserName::storage_size() == 32);

}

using namespace zsl::types::tests;

TEST_CASE("estring tests", "[types_tests/estring]")
{
    SECTION("basics")
    {
        REQUIRE(true);
    }
}
