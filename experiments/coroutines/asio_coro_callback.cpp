#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/detail/error_code.hpp>
#include <ranges>
#include <utility>
#include <thread>
#include <boost/asio.hpp>
#include <coroutine>
#include <iostream>
#include <string>
#include <type_traits>
#include <functional>
#include <expected>
#include <coroutine>
#include <chrono>
#include <filesystem>
#include <format>
#include <source_location>

#include <boost/asio/post.hpp>
#include <unistd.h>

namespace asio = boost::asio;

struct fmt_t
{
    char const * const str_;
    std::source_location const loc_;

    consteval fmt_t(char const * str, std::source_location const loc = std::source_location::current()) : str_{str}, loc_{loc}
    {
    }
};

inline std::string & log_buffer()
{
    thread_local std::string buffer;
    buffer.clear();
    return buffer;
}

template < typename T >
constexpr bool is_coroutine_handle_v = false;

template < typename T >
constexpr bool is_coroutine_handle_v < std::coroutine_handle < T > > = true;

template < typename Arg >
decltype(auto) prettify(Arg && v)
{
    using A = std::decay_t < Arg >;
    if constexpr (std::is_pointer_v < A >)
        return (void const *)v;
    else
    if constexpr (is_coroutine_handle_v < A >)
        return prettify(v.address());
    else
        return v;
};

template < typename... Args >
inline auto log(fmt_t const & fmt, Args &&... args)
{
    auto & buffer = log_buffer();

    auto const & [str, loc] = fmt;
    std::format_to(
        std::format_to(std::back_inserter(buffer), "[{}][{}:{}][{}] - ", std::chrono::high_resolution_clock::now(), std::filesystem::path(loc.file_name()).filename().c_str(), loc.line(), loc.function_name()),
        std::runtime_format(str), prettify(std::forward < Args >(args))...
    );

    std::clog << buffer << std::endl;
}

template < typename Context, typename... Args >
inline auto logc(Context && ctx, fmt_t const & fmt, Args &&... args)
{
    auto const & [str, loc] = fmt;
    auto & buffer = log_buffer();
    std::format_to(
        std::format_to(std::back_inserter(buffer), "[{}][{}:{}][{}] - [{}]: ", std::chrono::high_resolution_clock::now(), std::filesystem::path(loc.file_name()).filename().c_str(), loc.line(), loc.function_name(), prettify(ctx)),
        std::runtime_format(str), prettify(std::forward < Args >(args))...
    );

    std::clog << buffer << std::endl;
}

template < typename T >
struct Awaitable;

struct AwaitableTaskBase
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
struct AwaitableResult
{
    void set(T && v)
    {
        v_ = std::expected(std::move(v));
    }

    T get()
    {
        return std::exchange(v_, std::unexpected(-1)).value();
    }

    void return_value(T && v)
    {
        set(std::move(v));
    }
    
    using Value = std::expected < T, bool >;
    Value v_{std::unexpected(-1)};
};

template <>
struct AwaitableResult < void >
{
    void return_void()
    {
    }
};

template < typename T = void >
struct AwaitableTask : AwaitableTaskBase, AwaitableResult < T >
{
    using coroutine_t = std::coroutine_handle < AwaitableTask >;
    coroutine_t coroutine_{nullptr};

    template < typename... Args >
    AwaitableTask(Args &&...)
    {
        logc(this, "Creating AwaitableTask...");
    }

    AwaitableTask()
    {
        logc(this, "Constructing...");
    }

    Awaitable < T > get_return_object()
    {
        auto coroutine = coroutine_t::from_promise(*this);
        coroutine_ = coroutine;
        logc(this, "Creating... coroutine_ = {}", coroutine_);
        return Awaitable < T > {coroutine};
    }
};

template < typename T >
struct Awaitable
{
    using coroutine_t = AwaitableTask < T >::coroutine_t;
    coroutine_t coroutine_{};

    bool await_ready()
    {
        logc(this, "Await ready...");
        return false;
    }

    template < typename U >
    void await_suspend(std::coroutine_handle < AwaitableTask < U > > continuation)
    {
        coroutine_.promise().continuation_ = continuation;
        log("Suspending... coroutine_ = {} coroutine_.continuation_ = {}", coroutine_, coroutine_.promise().continuation_);
    }

    auto await_resume()
    {
        logc(this, "Resuming...");
        if constexpr (!std::is_same_v < T, void >)
        {
            T v{};
            if (v.has_value())
                logc(this, "Got... value {}", v.value());
            else
                logc(this, "Got... error {}", v.error());
            return std::move(*v);
        }
    }
};

template < typename T >
struct ASIOPostAwaitable
{
    asio::io_context & io_;
    std::function < T () > func_;
    std::shared_ptr < T > value_ = std::make_shared < T >();

    bool await_ready() const noexcept
    {
        return false;
    }
    
    template < typename U >
    void await_suspend(std::coroutine_handle < AwaitableTask < U > > h) const
    {
        asio::post(
          io_
        , [this, h, f = std::move(func_)] mutable
          {
              *value_ = f();
              h.resume();
          }
        );
    }

    T await_resume() const noexcept
    {
        if constexpr (!std::is_same_v < T, void >)
            return std::move(*value_);
    }
};

template <>
struct ASIOPostAwaitable < void >
{
    asio::io_context & io_;
    std::function < void () > func_;

    bool await_ready() const noexcept
    {
        return false;
    }
    
    template < typename U >
    void await_suspend(std::coroutine_handle < AwaitableTask < U > > h) const
    {
        asio::post(
          io_
        , [this, h, f{func_}] mutable
          {
              f();
              h.resume();
          }
        );
    }

    void await_resume() const noexcept
    {
    }
};

template < typename T, typename ... Args >
struct std::coroutine_traits < Awaitable < T >, Args... >
{
    using promise_type = AwaitableTask < T >;
};

template < typename T, typename... Args >
struct std::coroutine_traits < ASIOPostAwaitable < T >, Args... >
{
    using promise_type = AwaitableTask < T >;
};

template < typename F >
auto post(asio::io_context& io, F && f)
{
    return ASIOPostAwaitable < std::invoke_result_t < F > >{io, std::move(f)};
}

auto int_coroutine(asio::io_context& io)
{
    std::cout << "int_coroutine started" << std::endl;
    return ASIOPostAwaitable < int >{io, [] {
        std::cout << "int_coroutine callback computing value    " << std::endl;
        return 123;
    }};
}

auto string_coroutine(asio::io_context& io)
{
    std::cout << "string_coroutine started" << std::endl;
    return ASIOPostAwaitable<std::string>{io, []{
        std::cout << "string_coroutine callback computing value" << std::endl;
        return std::string("hello world");
    }};
}

auto void_coroutine(asio::io_context& io)
{
    std::cout << "void_coroutine started" << std::endl;
    return ASIOPostAwaitable<void>{io, [] {
        std::cout << "void_coroutine callback" << std::endl;
    }};
}

struct Cache
{
    asio::io_context & io_;
    char buf[4] = {};

    template < typename F >
    auto fetch(int key, F && f)
    {
        try
        {
            using namespace asio::ip;
            tcp::socket socket(io_);
            socket.connect(tcp::endpoint(address_v4::loopback(), 56789));
            if (auto const & received = asio::read(socket, asio::buffer(buf, sizeof(buf)), asio::transfer_exactly(sizeof(int32_t))))
            {
                if (received == sizeof(int32_t))
                {
                    std::cout << "Cache fetch for key: " << key << std::endl;
                    f(key, std::to_string(*reinterpret_cast<int32_t const *>(buf)));
                }
                else
                {
                    // log("Got {} error & {} bytes", ec.message(), received);
                    log("Got {} bytes", received);
                    f(key, std::nullopt);
                }
            }
        }
        catch (boost::system::system_error const & e)
        {
            f(key, std::nullopt);
        }
    }
};

Awaitable < void > run_coroutines(asio::io_context& io)
{
    [[maybe_unused]] auto x = co_await post(io,
                    [] () {
                        std::cout << "Post callback executed" << std::endl;
                        return 42; // Example value
                    });
    auto t1 = co_await int_coroutine(io);
    auto t2 = co_await string_coroutine(io);
    co_await void_coroutine(io);

    std::cout << "int_coroutine returned: " << t1 << "" << std::endl;
    std::cout << "string_coroutine returned: " << t2 << "" << std::endl;
    std::cout << "void_coroutine returned" << std::endl;

    Cache c{io};
    co_await post(io, [&c] { c.fetch(42, [](int key, std::optional < std::string > value) {
        std::cout << "Cache fetch completed for key: " << key << ", value: " << value.value_or("<null>") << std::endl;
    }); });
    log("Cache fetch co_await over");
    co_return;
}

void run_timer(asio::io_context & io)
{
    asio::steady_timer timer(io, std::chrono::seconds(1));
    timer.async_wait([](const std::error_code& ec) {
        if (!ec) {
            std::cout << "Timer expired!" << std::endl;
        } else {
            std::cerr << "Timer error: " << ec.message() << std::endl;
        }
    });
}

int main()
{
    asio::io_context io;
    // Run io_context in a separate thread
    std::thread io_thread([&io]{ io.run(); });
    asio::post(io, [&io] () { run_coroutines(io); });
    io_thread.join();
    return 0;
}
