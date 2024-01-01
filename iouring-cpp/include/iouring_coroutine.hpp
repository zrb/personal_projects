#pragma once

#include "iouring_service.hpp"

#include <logging/logging.hpp>

#include <coroutine>
#include <expected>

namespace zsl::iouring::coroutine
{

template < typename T >
struct awaitable_t;

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

struct ring_awaitable_base_t
{
    ring_t & ring_;
    ring_t::event_t & event_;
};

template < typename E >
struct ring_awaitable_event_data_t
{
    E e_;
};

template < typename T, typename E >
// struct ring_awaitable_t : ring_awaitable_event_data_t < E >, ring_awaitable_base_t
struct ring_awaitable_t : ring_awaitable_base_t
{
    E e_;
 
    template < typename... Args >
    constexpr explicit ring_awaitable_t(ring_t & ring, Args &&... args) : ring_awaitable_base_t{ring, e_}, e_{std::forward < Args >(args)...}
    {
    }

    constexpr ring_awaitable_t(ring_awaitable_t && rhs) : ring_awaitable_base_t{std::move(rhs.ring_), this->e_}, e_{std::move(rhs.e_)}
    {
    }

    constexpr ring_awaitable_t(ring_awaitable_t const & rhs) : ring_awaitable_base_t{rhs.ring_, this->e_}, e_{rhs.e_}
    {
    }

    void submit();

    constexpr bool await_ready()
    {
        logc(this, "Await ready... ring_ = {} event_ = {} event_.coroutine_ = {} event_.handler_ = {}", &ring_, &event_, event_.coroutine_, event_.handler_);
        return false;
    }

    template < typename U >
    void await_suspend(typename std::coroutine_handle < U > coroutine)
    {
        event_.coroutine_ = coroutine;
        submit();
        logc(this, "Suspending ... ring_ = {} event_ = {} event_.coroutine_ = {} event_.handler_ = {}", &ring_, &event_, event_.coroutine_, event_.handler_);
    }

    T await_resume()
    {
        logc(this, "Resuming... ring_ = {} event_ = {} event_.coroutine_ = {} event_.handler_ = {}", &ring_, &event_, event_.coroutine_, event_.handler_);
        if constexpr (!std::is_same_v < T, void >)
        {
            T v = std::exchange(this->e_.response_.result_, std::unexpected(-1));
            if (v.has_value())
                logc(this, "Got... value {}", v.value());
            else
                logc(this, "Got... error {}", v.error());
            return v;
        }
    }
};

}

namespace std
{

using zsl::iouring::coroutine::awaitable_task_t;
using zsl::iouring::coroutine::awaitable_t;
using zsl::iouring::coroutine::ring_awaitable_t;

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

}
