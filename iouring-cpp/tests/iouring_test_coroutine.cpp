#include <iouring.hpp>
#include <logging/logging.hpp>

#include <catch2/catch_all.hpp>

#include <iostream>

using zsl::logging::log;

using namespace zsl::iouring;
using namespace zsl::iouring::scheduler;

ring_t ring;
scheduler_t sched{ring};
int32_t count{};

template < typename C >
constexpr auto to_timespec(std::chrono::time_point < C > when)
{
    auto const nsecs_since_epoch = std::chrono::duration_cast < std::chrono::nanoseconds >(when.time_since_epoch());
    auto const secs_since_epoch = std::chrono::duration_cast < std::chrono::seconds >(nsecs_since_epoch);
    auto const nsecs = nsecs_since_epoch - secs_since_epoch;
    return __kernel_timespec{ .tv_sec = secs_since_epoch.count(), .tv_nsec = nsecs.count() };
}

template < typename R, typename P >
constexpr auto to_timespec(std::chrono::duration < R, P > interval)
{
    auto const nsecs_since_epoch = std::chrono::duration_cast < std::chrono::nanoseconds >(interval);
    auto const secs_since_epoch = std::chrono::duration_cast < std::chrono::seconds >(nsecs_since_epoch);
    auto const nsecs = nsecs_since_epoch - secs_since_epoch;
    return __kernel_timespec{ .tv_sec = secs_since_epoch.count(), .tv_nsec = nsecs.count() };
}

TEST_CASE("iouring coroutine tests", "iouring coroutine tests")
{
    SECTION("coroutine/timer/single")
    {
        log("--------------------------------------------------------------------");
        auto f = [] (scheduler_t & scheduler, int32_t & c) -> zsl::iouring::awaitable_t < void >
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
        auto f = [] (scheduler_t & scheduler, int32_t & c) -> zsl::iouring::awaitable_t < void >
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
