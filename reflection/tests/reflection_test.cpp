#include <catch2/catch_all.hpp>

#include <reflection/reflection.hpp>

using namespace zsl::reflection;

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

        zsl::reflection::reflect < Address >(
            "Address",
            {
                {"postCode", &Address::postCode_},
                {"doorNumber", &Address::doorNumber_},
            }
        );

        zsl::reflection::reflect < User >(
            "User",
            {
                {"name", &User::name_},
                {"age", &User::age_},
                {"homeAddress", &User::homeAddress_},
            }
        );
        REQUIRE(true);
    }
}

TEST_CASE("reflection tests", "[reflection_tests]")
{
    SECTION("basics")
    {
        
        struct User
        {
            
        };
        REQUIRE(true);
    }
}
