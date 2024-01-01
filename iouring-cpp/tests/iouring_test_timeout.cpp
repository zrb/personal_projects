#include <iouring.hpp>

#include <catch2/catch_all.hpp>

#include <format>
#include <iostream>

#include <mcheck.h>

using zsl::iouring::ring_t;
using zsl::iouring::scheduler::scheduler_t;

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

TEST_CASE("iouring timeout tests", "iouring timeout tests")
{
    ring_t ring;
    static uint32_t count{0};
    SECTION("interval")
    {
        using interval_t = std::chrono::milliseconds;
        count = 0;
        
        auto f = [&ring] (auto && ts, auto && te)
        {
            io_uring_sqe * sqe{};
            sqe = ring.prepare(&io_uring_prep_timeout, &ts, 0, 0);
            ring.submit(sqe, &te);
            sqe = ring.prepare(&io_uring_prep_timeout, &ts, 0, 0);
            ring.submit(sqe, &te);
            sqe = ring.prepare(&io_uring_prep_timeout, &ts, 0, 0);
            ring.submit(sqe, &te);
        };
        ring_t::event_t te{+[] (io_uring_cqe *, ring_t::event_t &) { std::cout << std::format("timed out @ {}", std::chrono::high_resolution_clock::now()) << std::endl; count++; } };
        f(to_timespec(interval_t{5}), te);
        ring.wait_for_events(1, interval_t{10});
        ring.wait_for_events(1, interval_t{10});
        ring.wait_for_events(1, interval_t{10});
        REQUIRE(count == 3);
    }
}


