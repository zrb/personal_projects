#include <iouring.hpp>
#include <iouring_utils_time.hpp>

#include <catch2/catch_all.hpp>

#include <format>
#include <iostream>

#include <mcheck.h>

using zsl::iouring::ring_t;
using zsl::iouring::scheduler::scheduler_t;

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
            ring.prepare(te, &io_uring_prep_timeout, &ts, 0, 0);
            ring.prepare(te, &io_uring_prep_timeout, &ts, 0, 0);
            ring.prepare(te, &io_uring_prep_timeout, &ts, 0, 0);
            ring.submit();
        };
        ring_t::event_t te{+[] (io_uring_cqe *, ring_t::event_t &) { std::cout << std::format("timed out @ {}", std::chrono::high_resolution_clock::now()) << std::endl; count++; } };
        f(zsl::iouring::utils::time::to_timespec(interval_t{5}), te);
        ring.wait_for_events(1, interval_t{10});
        ring.wait_for_events(1, interval_t{10});
        ring.wait_for_events(1, interval_t{10});
        REQUIRE(count == 3);
    }
}


