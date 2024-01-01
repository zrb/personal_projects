#include "iouring_net.hpp"
#include "iouring_impl.hpp"

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
using namespace zsl::iouring::net;

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
    ring_->prepare(ce, &io_uring_prep_cancel_fd, std::to_underlying(fd_), IORING_ASYNC_CANCEL_ALL);
    ring_->submit();
    logc(*this, "Closing...");
    return 0 != ::close(std::to_underlying(std::exchange(fd_, invalid_socket_fd)));
}

bool socket_t::bind(ipaddressv4_t const ip, ipport_t const port)
{
    auto sa = to_sockaddr(ip, port);
    return 0 == ::bind(std::to_underlying(fd_), std::bit_cast < sockaddr * >(&sa), sizeof(sa));
}

socket_t::send_awaitable_t socket_t::send(std::span < uint8_t const > const buf)
{
    logc(*this, "Send starting... this = {} ", this);
    return send_awaitable_t {
            ring(),
            send_event_t
            {
                {&on_send},
                {.self_ = *this},
                { buf },
                {}
            }
           };
}

void socket_t::on_send(io_uring_cqe * cqe, ring_t::event_t & e)
{
    if (cqe->res > 0)
    {
        auto & se = static_cast < send_event_t & >(e);
        logc(se.context_.self_, "Sent {} bytes...", cqe->res);
        auto h = e.coroutine_;
        std::exchange(se.response_.result_, send_result_t{std::move(cqe->res)});
        h.resume();
    }
}

socket_t::recv_awaitable_t socket_t::recv(std::span < uint8_t > buf)
{
    logc(*this, "Receive starting... this = {}", this);
    return recv_awaitable_t {
            ring(),
            recv_event_t
            {
                {&on_recv},
                {.self_ = *this},
                { buf },
                {}
            }
           };
}

void socket_t::on_recv(io_uring_cqe * cqe, ring_t::event_t & e)
{
    recv_event_t & re = static_cast < recv_event_t & >(e);
    log("fd[{}]: On receive.. event = {} self_ = {} coroutine = {}", re.context_.self_, &re, &re.context_.self_, re.coroutine_);
    if (cqe->res > 0)
    {
        log("fd[{}]: Received {} bytes...", re.context_.self_, cqe->res);
        std::exchange(re.response_.result_, recv_result_t{std::move(cqe->res)});
    }
    else
    if (cqe->res == 0)
    {
        log("fd[{}]: Closed...", re.context_.self_);
        std::exchange(re.response_.result_, recv_result_t{std::unexpected(std::move(cqe->res))});
    }
    else
    {
        log("fd[{}]: Failed with... {}", re.context_.self_, cqe->res);
        std::exchange(re.response_.result_, recv_result_t{std::unexpected(std::move(cqe->res))});
    }
    e.coroutine_.resume();
    // h.destroy();
}

bool tcp_socket_t::listen()
{
    return 0 == ::listen(std::to_underlying(fd_), 8);
}

tcp_socket_t::acceptor_t::accept_awaitable_t tcp_socket_t::acceptor_t::accept()
{
    logc(*this, "Accept starting... this = {} handler = {} ", this, &on_accept);
    return accept_awaitable_t {
            ss_.ring(),
            acceptor_t::accept_event_t
            {
                {&on_accept},
                {.self_ = *this},
                {},
                {}
            }
           };
}

void tcp_socket_t::acceptor_t::on_accept(io_uring_cqe * cqe, ring_t::event_t & e)
{
    auto & ae = static_cast < accept_event_t & >(e);
    logc(ae.context_.self_, "Accept complete... this = {} event = {} handler = {} coroutine = {} result = {}", ae.context_.self_, &ae, ae.handler_, ae.coroutine_, cqe->res);
    if (cqe->res > 0)
    {
        socket_fd_t fd{cqe->res};
        tcp_socket_t cs{ae.context_.self_.ring_, fd};
        std::exchange(ae.response_.result_, accept_result_t{std::move(cs)});
        e.coroutine_.resume();
    }
}

socket_t::connect_awaitable_t socket_t::connect(ipaddressv4_t const & ip, ipport_t const & port)
{
    logc(*this, "Connect starting... this = {} handler = {}", this, &on_connect);
    return connect_awaitable_t {
            ring(),
            connect_event_t
            {
                {&on_connect},
                {.self_ = *this},
                {.ip_ = ip, .port_ = port},
                {}
            }
           };
}

void socket_t::on_connect(io_uring_cqe * cqe, ring_t::event_t & e)
{
    auto & ce = static_cast < connect_event_t & >(e);
    log("On connect... connect return = {} event = {} coroutine = {} this = {}", cqe->res, &ce, ce.coroutine_, &ce.context_.self_);
    if (cqe->res == 0)
    {
        log("Connected... event = {} coroutine = {} this = {}", &ce, ce.coroutine_, &ce.context_.self_);
        std::exchange(ce.response_.result_, connect_status_t::SUCCEEDED);
    }
    else
    {
        log("Connect failed... event = {} coroutine = {} this = {}", &ce, ce.coroutine_, &ce.context_.self_);
        std::exchange(ce.response_.result_, connect_status_t::FAILED);

    }
    e.coroutine_.resume();
}

}

namespace zsl::iouring
{

using namespace net;
using net::socket_t;

template <>
void socket_t::connect_awaitable_t::submit()
{
    auto sa = to_sockaddr(e_.request_.ip_, e_.request_.port_);
    ring_.prepare(e_, &io_uring_prep_connect, std::to_underlying(e_.context_.self_.fd()), (sockaddr *)&sa, sizeof(sa));
    ring_.submit();
}

template <>
void socket_t::send_awaitable_t::submit()
{
    ring_.prepare(e_, &io_uring_prep_send, std::to_underlying(e_.context_.self_.fd()), e_.request_.buf_.data(), e_.request_.buf_.size(), 0);
    ring_.submit();
}

template <>
void socket_t::recv_awaitable_t::submit()
{
    ring_.prepare(e_, &io_uring_prep_recv, std::to_underlying(e_.context_.self_.fd()), e_.request_.buf_.data(), e_.request_.buf_.size(), 0);
    ring_.submit();
}

template <>
void tcp_socket_t::acceptor_t::accept_awaitable_t::submit()
{
    ring_.prepare(e_, &io_uring_prep_multishot_accept, std::to_underlying(e_.context_.self_.socket().fd()), (sockaddr *)nullptr, (uint32_t *)nullptr, 0);
    ring_.submit();
}

}
