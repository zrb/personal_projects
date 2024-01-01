#pragma once

#include <chrono>

#include <unistd.h>

namespace zsl::iouring::utils::time
{

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

}
