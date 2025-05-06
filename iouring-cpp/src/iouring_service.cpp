#include "iouring.hpp"

#include "iouring_impl.hpp"

#include <logging/logging.hpp>

#include <cstdint>
#include <chrono>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <utility>
#include <stdexcept>

import zsl.types.bitset;

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

}

namespace zsl::iouring
{

ring_t::ring_t() : impl_(std::make_unique < impl_t >(*this))
{
}

ring_t::~ring_t() = default;

void ring_t::submit()
{
    return impl_->submit();
}

void ring_t::wait_for_events(size_t const count, duration_t const wait_timeout)
{
    return impl_->wait_for_events(count, wait_timeout);
}

}
