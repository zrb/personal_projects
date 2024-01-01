#include <iouring.hpp>
#include <logging/logging.hpp>

#include <catch2/catch_all.hpp>

#include <iostream>

using zsl::logging::log;

using namespace zsl::iouring;
using namespace zsl::iouring::scheduler;
using namespace zsl::iouring::coroutine;

ring_t ring;
scheduler_t sched{ring};
int32_t count{};

TEST_CASE("iouring coroutine tests", "iouring coroutine tests")
{
    SECTION("coroutine/timer/single")
    {
        auto a = sched.create_timer(std::chrono::seconds(1));
        auto b = a;
        log("&a.e_ = {} &a.event_ = {} &b.e_ = {} &b.event_ = {} ", &a.e_, &a.event_, &b.e_, &b.event_);
        log("--------------------------------------------------------------------");
        auto f = [] (scheduler_t & scheduler, int32_t & c) -> awaitable_t < void >
        {
            log("***** Begin *****");
            co_await scheduler.create_timer(std::chrono::seconds(1));
            ++c;
            log("***** Done *****");
        };
        int32_t c{};
        f(sched, c);
        ring.wait_for_events(1, std::chrono::seconds(2));
        REQUIRE(c == 1);
    }
    SECTION("coroutine/timer/multiple")
    {
        log("--------------------------------------------------------------------");
        auto f = [] (scheduler_t & scheduler, int32_t & c) -> awaitable_t < void >
        {
            for (auto i = 0; i < 5; ++i)
            {
                log("***** Begin *****");
                co_await scheduler.create_timer(std::chrono::seconds(1));
                ++c;
                log("***** Done *****");
            }
        };
        int32_t c{};
        f(sched, c);
        for (auto i = 0; i < 5; ++i)
            ring.wait_for_events(1, std::chrono::seconds(6));
        REQUIRE(c == 5);
    }
}
