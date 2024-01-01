#include <chrono>
#include <coroutine>
#include <expected>
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

#include <types/estring.hpp>
#include <logging/logging.hpp>

struct io_uring_cqe;
struct io_uring_sqe;

namespace zsl::iouring
{

template < typename T, typename E >
using expected_t = std::expected < T, E >;

using namespace std::literals::chrono_literals;

using highres_clock_t = std::chrono::high_resolution_clock;
using timepoint_t = std::chrono::time_point < highres_clock_t >;
using duration_t = std::chrono::microseconds;

using zsl::logging::log;
using zsl::logging::logc;

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
    io_uring_sqe * prepare(F && f, Args &&... args);

    void submit(io_uring_sqe * sqe, event_t * e);

  private:
    struct impl_t;
    std::unique_ptr < impl_t > impl_;
};

template < typename T >
struct awaitable_t;

template < typename T, typename E >
struct ring_awaitable_t;

struct awaitable_task_base_t
{
    auto initial_suspend()
    {
        logc(this, "Initial suspend...");
        return std::suspend_never{};
    }

    auto final_suspend() noexcept
    {
        logc(this, "Final suspend...");
        return std::suspend_never{};
    }

    void unhandled_exception()
    {
    }

    std::coroutine_handle<> continuation_{};
};

template < typename T >
struct awaitable_result_t
{
    void set(T && v)
    {
        v_ = std::move(v);
    }

    T get()
    {
        return std::exchange(v_, std::unexpected(-1)).value();
    }

    void return_value(T && v)
    {
        set(std::move(v));
    }
    
    T v_{std::unexpected(-1)};
};

template <>
struct awaitable_result_t < void >
{
    void return_void()
    {
    }
};

template < typename T = void >
struct awaitable_task_t : awaitable_task_base_t, awaitable_result_t < T >
{
    using coroutine_t = std::coroutine_handle < awaitable_task_t >;
    coroutine_t coroutine_{nullptr};

    template < typename... Args >
    awaitable_task_t(Args &&...)
    {
        logc(this, "Creating task...");
    }

    awaitable_task_t()
    {
        logc(this, "Constructing...");
    }

    awaitable_t < T > get_return_object()
    {
        auto coroutine = coroutine_t::from_promise(*this);
        coroutine_ = coroutine;
        logc(this, "Creating... coroutine_ = {}", coroutine_);
        return awaitable_t < T > {coroutine};
    }
};

struct ring_awaitable_base_t
{
    ring_t * ring_{};
    io_uring_sqe * sqe_{};
    ring_t::event_t * event_{};
};

template < typename T >
struct awaitable_t
{
    using coroutine_t = awaitable_task_t < T >::coroutine_t;
    coroutine_t coroutine_{};

    bool await_ready()
    {
        logc(this, "Await ready...");
        return false;
    }

    template < typename U >
    void await_suspend(std::coroutine_handle < awaitable_task_t < U > > continuation)
    {
        coroutine_.promise().continuation_ = continuation;
        log("Suspending... coroutine_ = {} coroutine_.continuation_ = {}", coroutine_, coroutine_.promise().continuation_);
    }

    void await_resume()
    {
        logc(this, "Resuming...");
        // if constexpr (!std::is_same_v < T, void >)
        // {
        //     T * v{};
        //     // if (v.has_value())
        //     //     logc(this, "Got... value {}", v.value());
        //     // else
        //     //     logc(this, "Got... error {}", v.error());
        //     return std::move(*v);
        // }
    }
};

template < typename T, typename E >
struct ring_awaitable_t : ring_awaitable_base_t
{
    bool await_ready()
    {
        logc(this, "Await ready... ring_ = {} sqe = {} event_ = {} event_.coroutine_ = {} event_.handler_ = {}", ring_, sqe_, event_, event_->coroutine_, event_->handler_);
        return false;
    }

    template < typename U >
    void await_suspend(typename std::coroutine_handle < U > coroutine)
    {
        event_->coroutine_ = coroutine;
        ring_->submit(sqe_, event_);
        logc(this, "Suspending ... ring_ = {} sqe = {} event_ = {} event_.coroutine_ = {} event_.handler_ = {}", ring_, sqe_, event_, event_->coroutine_, event_->handler_);
    }

    T await_resume()
    {
        logc(this, "Resuming... ring_ = {} sqe = {} event_ = {} event_.coroutine_ = {} event_.handler_ = {}", ring_, sqe_, event_, event_->coroutine_, event_->handler_);
        if constexpr (!std::is_same_v < T, void >)
        {
            auto & e = static_cast < E & >(*event_);
            T v = std::exchange(e.result_, std::unexpected(-1));
            if (v.has_value())
                logc(this, "Got... value {}", v.value());
            else
                logc(this, "Got... error {}", v.error());
            return v;
        }
    }
};

namespace scheduler
{

struct scheduler_t
{
    using timer_task_t = awaitable_task_t < void >;
    using timer_awaitable_t = ring_awaitable_t < void, void >;
    using event_t = ring_t::event_t;
    struct timer_event_t : ring_t::event_t
    {
        scheduler_t * self_{};
        __kernel_timespec ts_{};
    };

    ring_t & ring_;

    static void on_timeout(io_uring_cqe *, event_t & e);

    timer_event_t timer_event_{{&on_timeout, {}}, this};

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

namespace net
{

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
    
    socket_t(socket_t && rhs) noexcept
    {
        *this = std::move(rhs);
    }
    
    socket_t & operator = (socket_t && rhs) noexcept
    {
        if (this != &rhs)
        {
            this->close();
            this->ring_ = std::exchange(rhs.ring_, nullptr);
            this->fd_ = std::exchange(rhs.fd_, invalid_socket_fd);
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
        socket_t * self_;
        connect_result_t result_{std::unexpected(-1)};
    };
    connect_event_t connect_event_{{&socket_t::on_connect}, this};
    using connect_task_t = awaitable_task_t < connect_result_t >;
    using connect_awaitable_t = ring_awaitable_t < connect_result_t, connect_event_t >;

    connect_awaitable_t connect(ipaddressv4_t const & ip, ipport_t const & port);
    static void on_connect(io_uring_cqe * cqe, ring_t::event_t & e);

    using send_result_t = expected_t < ssize_t /* num bytes sent */, int32_t >;
    struct send_event_t : ring_t::event_t
    {
        socket_t * self_;
        send_result_t result_{std::unexpected(-1)};
    };

    using send_task_t = awaitable_task_t < send_result_t >;
    using send_awaitable_t = ring_awaitable_t < send_result_t, send_event_t >;

    send_event_t send_event_{{&socket_t::on_send}, this};

    static void on_send(io_uring_cqe * cqe, ring_t::event_t & e);

    send_awaitable_t send(uint8_t const * buf, uint32_t const bufsz);

    template < SizedBuffer T >
    auto send(T && buf)
    {
        return send(std::bit_cast < uint8_t const * >(std::ranges::data(buf)), std::ranges::size(buf));
    }

    using recv_result_t = expected_t < ssize_t /* num bytes received */, int32_t >;
    struct recv_event_t : ring_t::event_t
    {
        socket_t * self_;
        recv_result_t result_{std::unexpected(-1)};
    };
    recv_event_t recv_event_{{&socket_t::on_recv}, this};
    using recv_task_t = awaitable_task_t < recv_result_t >;
    using recv_awaitable_t = ring_awaitable_t < recv_result_t, recv_event_t >;
    
    static void on_recv(io_uring_cqe * cqe, ring_t::event_t & e);

    recv_awaitable_t recv(uint8_t * buf, uint32_t const bufsz);

    template < SizedBuffer T >
    auto recv(T & buf)
    {
        return recv(std::bit_cast < uint8_t  * >(std::ranges::data(buf)), std::ranges::size(buf));
    }

protected:
    ring_t * ring_{};
    socket_fd_t fd_{-1};
private:
    ring_t & ring()
    {
        return *ring_;
    }
    // template < typename T >
    // friend class coroutine::awaitable_task_t < T >::promise_base_t;
};

struct tcp_socket_t : socket_t
{
    using socket_t::socket_t;

    tcp_socket_t(ring_t & ring) : socket_t{ring, AF_INET, SOCK_STREAM, IPPROTO_TCP}
    {
    }

    tcp_socket_t(tcp_socket_t const &) = delete;
    tcp_socket_t & operator = (tcp_socket_t const &) = delete;
    
    tcp_socket_t(tcp_socket_t && rhs) : socket_t{std::move(rhs)}
    {
    }

    tcp_socket_t & operator = (tcp_socket_t && rhs)
    {
        static_cast < socket_t & >(*this) = static_cast < socket_t && >(rhs);
        return *this;
    }

    bool listen();

    struct acceptor_t;
    acceptor_t acceptor();
};

struct tcp_socket_t::acceptor_t
{
    using accept_result_t = expected_t < tcp_socket_t, int32_t >;
    struct accept_event_t : ring_t::event_t
    {
        acceptor_t * self_{};
        accept_result_t result_{std::unexpected(-1)};
    };
    accept_event_t accept_event_{{&acceptor_t::on_accept}, this};
    using accept_task_t = awaitable_task_t < accept_result_t >;
    using accept_awaitable_t = ring_awaitable_t < accept_result_t, accept_event_t >;

    accept_awaitable_t accept();
    static void on_accept(io_uring_cqe * cqe, ring_t::event_t & e);

    auto const & socket() const
    {
        return ss_;
    }

private:
    ring_t * ring_;
    tcp_socket_t & ss_;

    acceptor_t(ring_t * ring, tcp_socket_t & ss) : ring_{ring}, ss_{ss}
    {
    }

    friend class tcp_socket_t;
};

inline tcp_socket_t::acceptor_t tcp_socket_t::acceptor()
{
    return acceptor_t{ring_, *this};
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

}

namespace std
{

using namespace zsl::iouring;
using namespace zsl::iouring::net;

template <typename T, typename... Args>
struct coroutine_traits < awaitable_t < T >, Args... >
{
  typedef awaitable_task_t < T > promise_type;
};

template <typename T, typename E, typename... Args>
struct coroutine_traits < ring_awaitable_t < T, E >, Args... >
{
  typedef awaitable_task_t < T > promise_type;
};

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

