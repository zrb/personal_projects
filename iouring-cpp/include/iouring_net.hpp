#pragma once

#include "iouring_net.hpp"
#include "iouring_coroutine.hpp"

#include <ranges>

namespace zsl::iouring::net
{

template < typename T, typename E >
using expected_t = std::expected < T, E >;

//  networking
template < typename T >
concept Buffer = std::ranges::contiguous_range < T > && std::is_standard_layout_v < std::ranges::range_value_t < T > >;

template < typename T >
concept SizedBuffer = Buffer < T > && std::ranges::sized_range < T >;

enum class fd_t : int32_t;
constexpr fd_t const invalid_fd{-1};

enum class socket_fd_t : int32_t;
constexpr socket_fd_t const invalid_socket_fd{-1};

enum class ipport_t : uint16_t;

struct tag_ipaddressv4_t;
using ipaddressv4_t = types::typed_estring_t < tag_ipaddressv4_t, 7, INET_ADDRSTRLEN - 1 >;

constexpr ipaddressv4_t const IPADDRV4_ANY{"0.0.0.0"};
constexpr ipaddressv4_t const IPADDRV4_LOOPBACK{"127.0.0.1"};

struct tag_ipaddressv6_t;
using ipaddressv6_t = types::typed_bounded_estring_t < tag_ipaddressv6_t, INET6_ADDRSTRLEN - 1 >;

struct tag_hostname_t;
using hostname_t = types::typed_bounded_estring_t < tag_hostname_t, HOST_NAME_MAX - 1 >;

struct socket_t
{
protected:
    socket_t(ring_t & ring, socket_fd_t const fd) : ring_{&ring}, fd_{fd}
    {
    }
    ~socket_t();

public:
    socket_t(ring_t & ring, int const domain, int const type, int const protocol) : socket_t{ring, socket_fd_t{socket(domain, type, protocol)}}
    {
    }

    socket_t(socket_t const &) = delete;
    socket_t & operator = (socket_t const &) = delete;
    
    socket_t(socket_t && rhs) noexcept : ring_{rhs.ring_}, fd_{std::exchange(rhs.fd_, invalid_socket_fd)}
    {
    }
    
    socket_t & operator = (socket_t && rhs)
    {
        if (this != &rhs)
        {
            //  ring_ = std::exchange(rhs.ring_, ring_);
            fd_ = std::exchange(rhs.fd_, fd_);
        }
        return *this;
    }

    constexpr auto fd() const
    {
        return fd_;
    }

    bool close();

    bool bind(ipaddressv4_t const ip, ipport_t const port);

    enum class connect_status_t : bool { FAILED, SUCCEEDED };
    using connect_result_t = expected_t < connect_status_t, int32_t >;
    struct connect_event_t : ring_t::event_t
    {
        struct context_t
        {
            socket_t & self_;
        };
        context_t context_;

        struct request_t
        {
            ipaddressv4_t ip_;
            ipport_t port_;
        };
        request_t request_;

        struct response_t
        {
            connect_result_t result_{std::unexpected(-1)};
        };
        response_t response_{};
    };

    using connect_task_t = coroutine::awaitable_task_t < connect_result_t >;
    using connect_awaitable_t = coroutine::ring_awaitable_t < connect_result_t, connect_event_t >;

    connect_awaitable_t connect(ipaddressv4_t const & ip, ipport_t const & port);
    static void on_connect(io_uring_cqe * cqe, ring_t::event_t & e);

    using send_result_t = expected_t < ssize_t /* num bytes sent */, int32_t >;
    struct send_event_t : ring_t::event_t
    {
        struct context_t
        {
            socket_t & self_;
        };
        context_t context_;

        struct request_t
        {
            std::span < uint8_t const > buf_{};
        };
        request_t request_{};

        struct response_t
        {
            send_result_t result_{std::unexpected(-1)};
        };
        response_t response_{};
    };
    using send_task_t = coroutine::awaitable_task_t < send_result_t >;
    using send_awaitable_t = coroutine::ring_awaitable_t < send_result_t, send_event_t >;

    static void on_send(io_uring_cqe * cqe, ring_t::event_t & e);

    send_awaitable_t send(std::span < uint8_t const > buf);

    template < SizedBuffer T >
    auto send(T && buf)
    {
        return send(std::span(std::bit_cast < uint8_t const * >(std::ranges::data(buf)), std::ranges::size(buf)));
    }

    using recv_result_t = expected_t < ssize_t /* num bytes received */, int32_t >;
    struct recv_event_t : ring_t::event_t
    {
        struct context_t
        {
            socket_t & self_;
        };
        context_t context_;

        struct request_t
        {
            std::span < uint8_t > buf_{};
        };
        request_t request_{};

        struct response_t
        {
            recv_result_t result_{std::unexpected(-1)};
        };
        response_t response_{};
    };

    using recv_task_t = coroutine::awaitable_task_t < recv_result_t >;
    using recv_awaitable_t = coroutine::ring_awaitable_t < recv_result_t, recv_event_t >;
    
    static void on_recv(io_uring_cqe * cqe, ring_t::event_t & e);

    recv_awaitable_t recv(std::span < uint8_t > buf);

    template < SizedBuffer T >
    auto recv(T & buf)
    {
        return recv(std::span(std::bit_cast < uint8_t  * >(std::ranges::data(buf)), std::ranges::size(buf)));
    }

protected:
    ring_t * ring_{};
    socket_fd_t fd_{-1};

    ring_t & ring()
    {
        return *ring_;
    }
};

struct tcp_socket_t : socket_t
{
    using socket_t::socket_t;

    tcp_socket_t(ring_t & ring) : socket_t{ring, AF_INET, SOCK_STREAM, IPPROTO_TCP}
    {
    }

    tcp_socket_t(tcp_socket_t const &) = delete;
    tcp_socket_t & operator = (tcp_socket_t const &) = delete;
    
    tcp_socket_t(tcp_socket_t &&) = default;
    tcp_socket_t & operator = (tcp_socket_t &&) = default;

    bool listen();

    struct acceptor_t;
    acceptor_t acceptor();
};

struct tcp_socket_t::acceptor_t
{
    using accept_result_t = expected_t < tcp_socket_t, int32_t >;
    struct accept_event_t : ring_t::event_t
    {
        struct context_t
        {
            acceptor_t & self_;
        };
        context_t context_;

        struct request_t
        {
        };
        request_t request_{};

        struct response_t
        {
            accept_result_t result_{std::unexpected(-1)};
        };
        response_t response_{};
    };
    using accept_task_t = coroutine::awaitable_task_t < accept_result_t >;
    using accept_awaitable_t = coroutine::ring_awaitable_t < accept_result_t, accept_event_t >;

    accept_awaitable_t accept();
    static void on_accept(io_uring_cqe * cqe, ring_t::event_t & e);

    auto const & socket() const
    {
        return ss_;
    }

private:
    ring_t & ring_;
    tcp_socket_t & ss_;

    acceptor_t(ring_t & ring, tcp_socket_t & ss) : ring_{ring}, ss_{ss}
    {
    }

    friend class tcp_socket_t;
};

inline tcp_socket_t::acceptor_t tcp_socket_t::acceptor()
{
    return acceptor_t{ring(), *this};
}

inline auto tcp_server(ring_t & ring, ipaddressv4_t const & ip, ipport_t const & port)
{
    tcp_socket_t s{ring};
    while (!s.bind(ip, port))
    {
        log("Couldn't bind...  will try in 5 seconds");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    if (!s.listen())
        throw std::runtime_error("Can't listen");
    return s;
}

}

namespace std
{

using namespace zsl::iouring::net;

template <>
struct formatter < socket_t > : std::formatter < int32_t >
{
    auto format(socket_t const & s, format_context & ctx) const
    {
        return formatter < int32_t >::format(std::to_underlying(s.fd()), ctx);
    }
};

template <>
struct formatter < tcp_socket_t::acceptor_t > : std::formatter < socket_t >
{
    auto format(tcp_socket_t::acceptor_t const & a, format_context & ctx) const
    {
        return formatter < socket_t >::format(a.socket(), ctx);
    }
};

template <>
struct formatter < tcp_socket_t > : std::formatter < socket_t >
{
};

template <>
struct formatter < socket_t::connect_status_t > : std::formatter < std::string >
{
    auto format(socket_t::connect_status_t const & s, format_context & ctx) const
    {
        return formatter < std::string >::format(s == socket_t::connect_status_t::SUCCEEDED ? "SUCCEEDED" : "FAILED", ctx);
    }
};

}
