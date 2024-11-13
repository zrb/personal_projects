#include <catch2/catch_all.hpp>

#include <types/string_literal.hpp>

namespace zsl::types::tests
{

static_assert(string_literal_t{"hello"}.length() == 5);

}

TEST_CASE("string literal tests", "[types_tests/string_literals]")
{
    SECTION("basics")
    {
        REQUIRE(true);
    }
}

