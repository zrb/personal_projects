#include <iouring.hpp>

#include <span>

namespace
{

using namespace zsl::iouring;
using namespace zsl::iouring::coroutine;
using namespace zsl::iouring::net;
using namespace zsl::iouring::scheduler;

awaitable_t < void > disconnect_if_idle(ring_t & ring, tcp_socket_t & cs, timepoint_t & lastMessageTime, bool & stopped)
{
    logc(cs, "Monitoring for inactivity...");
    scheduler_t scheduler{ring};
    while (!stopped)
    {
        logc(cs, "Waiting for timeout...");
        co_await scheduler.create_timer(std::chrono::seconds(3));
        logc(cs, "Timed out...");
        if (auto && now = highres_clock_t::now(); now - lastMessageTime > std::chrono::seconds(10))
        {
            logc(cs, "Disconnecting due to inactivity at {}.  Last message received at {}", now, lastMessageTime);
            stopped = true;
            cs.close();
        }
        else
        {
            logc(cs, "So far... so good.  Last message received at {}", lastMessageTime);
        }
    }
}

awaitable_t < void > handle_client(ring_t & ring, tcp_socket_t cs)
{
    std::array < uint8_t, 1024 > buffer;
    logc(cs, "Running client...");
    auto lastMessageTime{highres_clock_t::now()};
    bool stopped{false};
    auto dii = disconnect_if_idle(ring, cs, lastMessageTime, stopped);
    while (!stopped)
    {
        auto rr = co_await cs.recv(buffer);
        if (!rr.has_value())
            break;

        lastMessageTime = highres_clock_t::now();
        auto data = std::span(buffer.data(), rr.value());

        auto sr = co_await cs.send(data);
        if (!sr.has_value())
            break;
    }
    co_await dii;
}

awaitable_t < void > run_echo_server(ring_t & ring)
{
    auto s = tcp_server(ring, IPADDRV4_ANY, ipport_t{56789});
    while (true)
    {
        auto ar = co_await s.acceptor().accept();
        if (ar.has_value())
            handle_client(ring, std::move(ar.value()));
    }
}

}

int main()
{
    ring_t ring{};
    run_echo_server(ring);
    ring.run();
    return 0;
}
