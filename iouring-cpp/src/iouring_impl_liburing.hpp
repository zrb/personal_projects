#pragma once

#include "iouring_service.hpp"
#include "iouring_utils_time.hpp"

#include <liburing/io_uring.h>
#include <liburing.h>
#include <unistd.h>

namespace zsl::iouring::impl::liburing
{

using params_t = io_uring_params;

struct impl_t
{
    struct data_t
    {
        static io_uring liburing_init_ring(uint32_t const q_depth, params_t & /* params */)
        {
            io_uring ring{};
            if (auto const err = io_uring_queue_init(q_depth, &ring, 0); err == 0)
                return ring;
            else
                throw std::system_error(err, std::generic_category());
        }

        params_t params_{
            .sq_entries = 64,
            .cq_entries = 1024,
            .flags = IORING_SETUP_IOPOLL | IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF | IORING_SETUP_CQSIZE | IORING_SETUP_CLAMP | IORING_SETUP_SUBMIT_ALL | IORING_SETUP_COOP_TASKRUN,
            .sq_thread_cpu = 0b111000,
            .sq_thread_idle = 0,
            .features = 0,
            .wq_fd = 0,
            .resv = {},
            .sq_off = {},
            .cq_off = {},
        };

        uint32_t q_depth_{1024};
        io_uring ring_{liburing_init_ring(q_depth_, params_)};
    };

    ring_t & ring_;

    impl_t(ring_t & ring) : ring_{ring}
    {
    }

    template < typename F, typename... Args >
    auto prepare(ring_t::event_t & e, F && f, Args &&... args)
    {
        auto * sqe = io_uring_get_sqe(&data_.ring_);
        //  log("Preparing... ring = {} sqe = {}", &ring_, sqe);
        std::forward < F >(f)(sqe, std::forward < Args >(args)...);
        io_uring_sqe_set_data(sqe, &e);
    }

    void submit()
    {
        if (auto const r = io_uring_submit(&data_.ring_); r <= 0) [[unlikely]]
            throw std::system_error(r, std::generic_category(), "io_uring_submit");
    }
    
    void wait_for_events(size_t const count, std::chrono::nanoseconds const wait_timeout)
    {
        //  logc(&ring_, "Waiting for events...");
        io_uring_cqe * cqe = nullptr;
        auto ts = zsl::iouring::utils::time::to_timespec(wait_timeout);
        if (auto const r = io_uring_wait_cqes(&data_.ring_, &cqe, count, &ts, nullptr); r != 0) [[unlikely]]
        {
            switch (r)
            {
            case -ETIME:
                //  logc(&ring_, "Wait timed out...");
                return;
            default:
                throw std::system_error(r, std::generic_category(), "io_uring_wait_cqes");
            }
        }

        //  logc(&ring_, "Wait over...");
        uint32_t head{0};
        uint32_t completions{0};
        io_uring_for_each_cqe(&data_.ring_, head, cqe)
        {
            if (auto * p = io_uring_cqe_get_data(cqe); p)
            {
                //  log("Event... event = {}", p);
                if (p)
                {
                    auto & e = *reinterpret_cast < ring_t::event_t * >(p);
                    //  log("Calling handler... handler_ = {} event = {}", e.handler_, &e);
                    e.handler_(cqe, e);
                }
            }
            else
            {
                log("No event... {}", p);
            }
            completions++;
        }
        if (completions != 0)
            io_uring_cq_advance(&data_.ring_, completions);
    }

    data_t data_{};
};

}

