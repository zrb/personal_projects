#include <catch2/catch_all.hpp>

#include "../reflection.hpp"

TEST_CASE("reflection tests", "[reflection_tests]")
{
    SECTION("basics")
    {
        struct Address
        {
            std::string postCode_{};
            std::string doorNumber_{};
        };
        struct User
        {
            std::string name_{};
            int32_t age_{};
            Address homeAddress_{};
        };
        zrb::reflection::reflect("User", zrb::reflection::reflected_field_t{"name", &User::name_}, zrb::reflection::reflected_field_t{"age", &User::age_}, zrb::reflection::reflected_field_t{"homeAddress", &User::homeAddress_});
        REQUIRE(true);
    }
}
