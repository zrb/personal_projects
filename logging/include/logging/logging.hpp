#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <source_location>

namespace zsl::logging
{

struct fmt_t
{
    char const * const str_;
    std::source_location const loc_;

    fmt_t(char const * str, std::source_location const loc = std::source_location::current()) : str_{str}, loc_{loc}
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
        return v.address();
    else
        return v;
};

template < typename... Args >
inline auto log(fmt_t const & fmt, Args &&... args)
{
    auto & buffer = log_buffer();

    auto const & [str, loc] = fmt;
    std::vformat_to(
        std::format_to(std::back_inserter(buffer), "[{}][{}:{}][{}] - ", std::chrono::high_resolution_clock::now(), std::filesystem::path(loc.file_name()).filename().c_str(), loc.line(), loc.function_name()),
        str, std::make_format_args(prettify(std::forward < Args >(args))...)
    );

    std::clog << buffer << std::endl;
}

template < typename Context, typename... Args >
inline auto logc(Context && ctx, fmt_t const & fmt, Args &&... args)
{
    auto const & [str, loc] = fmt;
    auto & buffer = log_buffer();
    std::vformat_to(
        std::format_to(std::back_inserter(buffer), "[{}][{}:{}][{}] - [{}]: ", std::chrono::high_resolution_clock::now(), std::filesystem::path(loc.file_name()).filename().c_str(), loc.line(), loc.function_name(), prettify(ctx)),
        str, std::make_format_args(prettify(std::forward < Args >(args))...)
    );

    std::clog << buffer << std::endl;
}

}
