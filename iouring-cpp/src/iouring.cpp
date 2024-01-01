#include "iouring.hpp"

#include <types/bitset.hpp>
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

using params_t = io_uring_params;

namespace raw_io_uring
{

struct impl_t
{
    struct data_t
    {
        enum class fd_t : int32_t;
        constexpr static fd_t const invalid_fd{-1};

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
        
        fd_t fd_{invalid_fd};

        constexpr auto is_valid() const noexcept
        {
            return fd_ != invalid_fd;
        }


        data_t() : fd_{io_uring_setup(1024, &params_)}
        {
            if (!is_valid())
                throw std::runtime_error("Unable to create io_uring ring");
        }

        ~data_t()
        {
            if (is_valid())
                close(std::to_underlying(fd_));
        }
    };

    data_t data_{};
};

}

namespace liburing
{

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
    [[nodiscard]] auto prepare(F && f, Args &&... args)
    {
        auto * sqe = io_uring_get_sqe(&data_.ring_);
        log("Preparing... ring = {} sqe = {}", &ring_, sqe);
        std::forward < F >(f)(sqe, std::forward < Args >(args)...);
        return sqe;
    }

    void submit(io_uring_sqe * sqe, ring_t::event_t * event)
    {
        logc(&ring_, "Submitting... event.coroutine = {} event.handler_  {} sqe = {}", event->coroutine_, event->handler_, sqe);
        io_uring_sqe_set_data(sqe, event);
        if (auto const r = io_uring_submit(&data_.ring_); r <= 0) [[unlikely]]
            throw std::system_error(r, std::generic_category(), "io_uring_submit");
    }
    
    void wait_for_events(size_t const count, std::chrono::nanoseconds const wait_timeout)
    {
        logc(&ring_, "Waiting for events...");
        io_uring_cqe * cqe = nullptr;
        auto ts = to_timespec(wait_timeout);
        if (auto const r = io_uring_wait_cqes(&data_.ring_, &cqe, count, &ts, nullptr); r != 0) [[unlikely]]
        {
            switch (r)
            {
            case -ETIME:
                logc(&ring_, "Wait timed out...");
                return;
            default:
                throw std::system_error(r, std::generic_category(), "io_uring_wait_cqes");
            }
        }

        logc(&ring_, "Wait over...");
        uint32_t head{0};
        uint32_t completions{0};
        io_uring_for_each_cqe(&data_.ring_, head, cqe)
        {
            if (auto * p = io_uring_cqe_get_data(cqe); p)
            {
                log("Event... event = {}", p);
                if (p)
                {
                    auto & e = *reinterpret_cast < ring_t::event_t * >(p);
                    log("Calling handler... handler_ = {} event = {}", e.handler_, &e);
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

}

namespace zsl::iouring
{

namespace scheduler
{

void scheduler_t::on_timeout(io_uring_cqe *, event_t & e)
{
    auto & te = static_cast < timer_event_t & >(e);
    log("Timed out... {}", &te, te.handler_, te.self_);
    e.coroutine_.resume();
}


[[nodiscard]] scheduler_t::timer_awaitable_t scheduler_t::create_timer(duration_t const & interval)
{
    logc(this, "Creating timer... handler = {} self_ = {} on_timeout = {}", timer_event_.handler_, timer_event_.self_, &scheduler_t::on_timeout);
    auto & te = timer_event_;
    te.ts_ = to_timespec(interval);
    auto * sqe = ring_.prepare(&io_uring_prep_timeout, &te.ts_, 0, 0);
    return timer_awaitable_t{&ring_, sqe, &te};
}

// [[nodiscard]] scheduler_t::timer_awaitable_t scheduler_t::create_timer(timepoint_t const & when)
// {
//     log("Creating timer...");
//     auto & te = timer_event_;
//     auto ts = to_timespec(when);
//     auto * sqe = ring_.prepare(&io_uring_prep_timeout, &ts, 1, IORING_TIMEOUT_ETIME_SUCCESS | IORING_TIMEOUT_ABS);
//     return timer_awaitable_t{&ring_, sqe, &te};
// }

}

struct ring_t::impl_t : liburing::impl_t
{
};

ring_t::ring_t() : impl_(std::make_unique < impl_t >(*this))
{
}

ring_t::~ring_t() = default;

template < typename F, typename... Args >
[[nodiscard]] io_uring_sqe * ring_t::prepare(F && f, Args &&... args)
{
    return impl_->prepare(std::forward < F >(f), std::forward < Args >(args)...);
}

void ring_t::submit(io_uring_sqe * sqe, event_t * event)
{
    return impl_->submit(sqe, event);
}

void ring_t::wait_for_events(size_t const count, duration_t const wait_timeout)
{
    return impl_->wait_for_events(count, wait_timeout);
}

}

namespace zsl::iouring::net
{

socket_t::~socket_t()
{
    logc(*this, "Destructor...");
    close();
}

bool socket_t::close()
{
    struct close_event_t : ring_t::event_t
    {
    };
    static close_event_t ce{ { +[] (io_uring_cqe *, ring_t::event_t &) {} } };

    if (fd_ == invalid_socket_fd)
        return false;
    auto * sqe = ring_->prepare(&io_uring_prep_cancel_fd, std::to_underlying(fd_), IORING_ASYNC_CANCEL_ALL);
    ring_->submit(sqe, &ce);
    logc(*this, "Closing...");
    return 0 != ::close(std::to_underlying(fd_));
}

namespace
{

auto to_native(ipaddressv4_t const & ip)
{
    decltype(sockaddr_in::sin_addr) v;
    inet_aton(ip.c_str(), &v);
    return v;
}

auto to_native(ipport_t const & port)
{
    return ::htons(std::to_underlying(port));
}

auto to_sockaddr(ipaddressv4_t const & ip, ipport_t const & port)
{
    return sockaddr_in{ .sin_family = AF_INET, .sin_port = to_native(port), .sin_addr = to_native(ip), .sin_zero = 0 };
}

}

bool socket_t::bind(ipaddressv4_t const ip, ipport_t const port)
{
    auto sa = to_sockaddr(ip, port);
    return 0 == ::bind(std::to_underlying(fd_), std::bit_cast < sockaddr * >(&sa), sizeof(sa));
}

socket_t::send_awaitable_t socket_t::send(uint8_t const * buf, uint32_t const bufsz)
{
    auto & se = send_event_;
    auto * sqe = ring_->prepare(&io_uring_prep_send, std::to_underlying(fd_), buf, bufsz, 0);
    logc(*this, "Send starting... this = {} event = {} sqe = {}", this, &se, sqe);
    return send_awaitable_t{ring_, sqe, &se};
}

void socket_t::on_send(io_uring_cqe * cqe, ring_t::event_t & e)
{
    if (cqe->res > 0)
    {
        auto & se = static_cast < send_event_t & >(e);
        logc(*se.self_, "Sent {} bytes...", *se.self_, cqe->res);
        auto h = e.coroutine_;
        std::exchange(se.result_, send_result_t{std::move(cqe->res)});
        h.resume();
    }
}

socket_t::recv_awaitable_t socket_t::recv(uint8_t * buf, uint32_t const bufsz)
{
    auto * sqe = ring_->prepare(&io_uring_prep_recv, std::to_underlying(fd_), buf, bufsz, 0);
    auto & re = recv_event_;
    logc(*this, "Receive starting... this = {} event = {} sqe = {}", this, &re, sqe);
    return recv_awaitable_t{ring_, sqe, &re};
}

void socket_t::on_recv(io_uring_cqe * cqe, ring_t::event_t & e)
{
    recv_event_t & re = static_cast < recv_event_t & >(e);
    auto h = e.coroutine_;
    auto & self_ = *re.self_;
    log("fd[{}]: On receive.. event = {} coroutine = {}", *re.self_, (void const *)&e, h.address());
    if (cqe->res > 0)
    {
        log("fd[{}]: Received {} bytes...", self_, cqe->res);
        std::exchange(re.result_, recv_result_t{std::move(cqe->res)});
    }
    else
    if (cqe->res == 0)
    {
        log("fd[{}]: Closed...", self_);
        std::exchange(re.result_, recv_result_t{std::unexpected(std::move(cqe->res))});
    }
    else
    {
        log("fd[{}]: Failed with...", self_, cqe->res);
        std::exchange(re.result_, recv_result_t{std::unexpected(std::move(cqe->res))});
    }
    h.resume();
    // h.destroy();
}

bool tcp_socket_t::listen()
{
    return 0 == ::listen(std::to_underlying(fd_), 8);
}

tcp_socket_t::acceptor_t::accept_awaitable_t tcp_socket_t::acceptor_t::accept()
{
    auto & ae = accept_event_;
    ae.coroutine_ = {};
    ae.handler_ = &acceptor_t::on_accept;
    ae.self_ = this;
    auto * sqe = ring_->prepare(&io_uring_prep_multishot_accept, std::to_underlying(ss_.fd()), (sockaddr *)nullptr, (uint32_t *)nullptr, 0);
    logc(*this, "Accept starting... this = {} event = {} handler = {} coroutine = {}", ae.self_, &ae, ae.handler_, ae.coroutine_);
    return {ring_, sqe, &ae};
}

void tcp_socket_t::acceptor_t::on_accept(io_uring_cqe * cqe, ring_t::event_t & e)
{
    auto & ae = static_cast < accept_event_t & >(e);
    logc(*ae.self_, "Accept complete... this = {} event = {} handler = {} coroutine = {} result = {}", ae.self_, &ae, ae.handler_, ae.coroutine_, cqe->res);
    if (cqe->res > 0)
    {
        socket_fd_t fd{cqe->res};
        tcp_socket_t cs{*ae.self_->ring_, fd};
        auto h = e.coroutine_;
        std::exchange(ae.result_, accept_result_t{std::move(cs)});
        h.resume();
    }
}

socket_t::connect_awaitable_t socket_t::connect(ipaddressv4_t const & ip, ipport_t const & port)
{
    auto sa = to_sockaddr(ip, port);
    auto * sqe = ring_->prepare(&io_uring_prep_connect, std::to_underlying(fd_), (sockaddr *)&sa, sizeof(sa));
    auto & ce = connect_event_;
    logc(*this, "Connect starting... this = {} event = {} handler = {} sqe = {}", this, &ce, ce.handler_, sqe);
    return {ring_, sqe, &ce};
}

void socket_t::on_connect(io_uring_cqe * cqe, ring_t::event_t & e)
{
    auto & ae = static_cast < connect_event_t & >(e);
    auto h = e.coroutine_;
    auto && p = connect_task_t::coroutine_t::from_address(h.address()).promise();
    log("On connect... connect return = {} event = {} coroutine = {} this = {}", cqe->res, &ae, h.address(), ae.self_);
    if (cqe->res == 0)
    {
        log("Connected... event = {} coroutine = {} this = {}", &ae, h.address(), ae.self_);
        p.set(connect_status_t::SUCCEEDED);
    }
    else
    {
        log("Connect failed... event = {} coroutine = {} this = {}", &ae, h.address(), ae.self_);
        p.set(connect_status_t::FAILED);

    }
    h.resume();
}

}

template io_uring_sqe * zsl::iouring::ring_t::prepare < decltype(&io_uring_prep_nop) >(decltype(&io_uring_prep_nop) &&);
template io_uring_sqe * zsl::iouring::ring_t::prepare < decltype(&io_uring_prep_timeout), __kernel_timespec*, int, int >(decltype(&io_uring_prep_timeout) &&, __kernel_timespec* &&, int &&, int &&);