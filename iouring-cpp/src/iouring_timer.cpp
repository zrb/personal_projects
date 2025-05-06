#include "iouring.hpp"
#include "iouring_impl.hpp"

#include <logging/logging.hpp>

#include <cstdint>
#include <chrono>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <utility>
#include <stdexcept>

#include <liburing/io_uring.h>
#include <liburing.h>
#include <unistd.h>

import zsl.types.bitset;

namespace
{

using zsl::logging::log;
using namespace zsl::iouring;
using namespace zsl::iouring::scheduler;

using highres_clock_t = std::chrono::high_resolution_clock;
using nanos_t = std::chrono::nanoseconds;
using micros_t = std::chrono::microseconds;
using millis_t = std::chrono::milliseconds;
using secs_t = std::chrono::seconds;

}

namespace zsl::iouring::scheduler
{

void scheduler_t::on_timeout(io_uring_cqe *, ring_t::event_t & e)
{
    auto & te = static_cast < timer_event_t & >(e);
    log("Timed out... event = {} handler = {} self = {}", &te, te.handler_, &te.context_.self_);
    e.coroutine_.resume();
}

[[nodiscard]] scheduler_t::timer_awaitable_t scheduler_t::create_timer(duration_t const & interval)
{
    logc(this, "Creating timer... handler = {}", &scheduler_t::on_timeout);
    return timer_awaitable_t(
            ring_,
            timer_event_t
            {
                {&on_timeout},
                {.self_ = *this},
                {.ts_ = zsl::iouring::utils::time::to_timespec(interval)},
                {}
            }
    );
}

// [[nodiscard]] scheduler_t::timer_awaitable_t scheduler_t::create_timer(timepoint_t const & when)
// {
//     log("Creating timer...");
//     auto & te = timer_event_;
//     auto ts = to_timespec(when);
//     auto * sqe = ring_.prepare(&io_uring_prep_timeout, &ts, 1, IORING_TIMEOUT_ETIME_SUCCESS | IORING_TIMEOUT_ABS);
//     return timer_awaitable_t{&ring_, &te};
// }

}

namespace zsl::iouring
{

using namespace scheduler;

template<>
void scheduler_t::timer_awaitable_t::submit()
{
    logc(this, "Submitting timer...");
    ring_.prepare(e_, &io_uring_prep_timeout, &e_.request_.ts_, 0, 0);
    ring_.submit();
}

}

// template io_uring_sqe * zsl::iouring::ring_t::prepare < decltype(&io_uring_prep_nop) >(decltype(&io_uring_prep_nop) &&);
// template io_uring_sqe * zsl::iouring::ring_t::prepare < decltype(&io_uring_prep_timeout), __kernel_timespec*, int, int >(decltype(&io_uring_prep_timeout) &&, __kernel_timespec* &&, int &&, int &&);
