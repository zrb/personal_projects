#pragma once

#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <ranges>
#include <thread>
#include <utility>

#include <liburing/io_uring.h>
#include <liburing.h>

#include <linux/time_types.h>
#include <arpa/inet.h>

#include <logging/logging.hpp>

import zsl.types.estring;

struct io_uring_cqe;

namespace zsl::iouring
{

using namespace std::literals::chrono_literals;

using highres_clock_t = std::chrono::high_resolution_clock;
using timepoint_t = std::chrono::time_point < highres_clock_t >;
using duration_t = std::chrono::microseconds;

using ::zsl::logging::log;
using ::zsl::logging::logc;

struct ring_t
{
    ring_t();
    ~ring_t();
    ring_t(ring_t const &) = delete;
    ring_t & operator = (ring_t const &) = delete;
    ring_t(ring_t && rhs) = delete;
    ring_t & operator = (ring_t && rhs) = delete;

    struct event_t
    {
        using handler_t = void (*)(io_uring_cqe *, event_t &);
        handler_t handler_{};
        std::coroutine_handle<> coroutine_{};

        constexpr friend auto operator <=> (event_t const &, event_t const &) = default;
    };

    inline constexpr static duration_t default_wait_interval{1s};

    void wait_for_events(size_t const count = 1, duration_t const wait_timeout = default_wait_interval);

    template < typename R, typename P, typename D = std::chrono::duration < R, P > >
    void wait_for_events(size_t const count = 1, D const wait_timeout = D{default_wait_interval})
    {
        wait_for_events(count, duration_t{wait_timeout});
    }

    void run(bool & stopped)
    {
        while (!stopped)
            wait_for_events();
    }

    void run()
    {
        bool stopped{false};
        run(stopped);
    }

    template < typename F, typename... Args >
    void prepare(event_t & e, F && f, Args &&... args);

    void submit();

  private:
    struct impl_t;
    std::unique_ptr < impl_t > impl_;
};

}
