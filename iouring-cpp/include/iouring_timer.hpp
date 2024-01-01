#pragma once

#include "iouring_service.hpp"
#include "iouring_coroutine.hpp"

namespace zsl::iouring::scheduler
{

struct scheduler_t
{
    struct timer_event_t : ring_t::event_t
    {
        struct context_t
        {
            scheduler_t & self_;
        };
        context_t context_;

        struct request_t
        {
            __kernel_timespec ts_{};
        };
        request_t request_{};

        struct response_t
        {
        };
        response_t response_{};
    };
    using timer_awaitable_t = coroutine::ring_awaitable_t < void, timer_event_t >;
    using timer_task_t = coroutine::awaitable_task_t < void >;

    ring_t & ring_;

    static void on_timeout(io_uring_cqe *, ring_t::event_t & e);

    [[nodiscard]] timer_awaitable_t create_timer(duration_t const & interval);

    template < typename R, typename P, typename D = std::chrono::duration < R, P > >
    [[nodiscard]] auto create_timer(D const & interval)
    {
        return create_timer(duration_t{interval});
    }

    [[nodiscard]] timer_awaitable_t create_timer(timepoint_t const & when);

    template < typename C, typename D >
    [[nodiscard]] auto create_timer(std::chrono::time_point < C, D > const & when)
    {
        return create_timer(timepoint_t{when});
    }
};

}

