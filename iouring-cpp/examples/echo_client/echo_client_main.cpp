#include <iouring.hpp>

#include <span>

namespace
{

using namespace zsl::iouring;
using namespace zsl::iouring::coroutine;
using namespace zsl::iouring::net;
using namespace zsl::iouring::scheduler;

template < typename T, size_t SZ, typename L >
std::string_view to_string_view(std::array < T, SZ > const & v, L const len)
{
    return {std::bit_cast < char const * >(v.data()), size_t(len)};
}

awaitable_t < void > handle_server(tcp_socket_t ss)
{
    std::array < char, 4096 > buffer;
    for (auto s : {"hello", "world!", "coroutines", "are", "cool"})
    {
        std::string_view sent{s};
        auto && sr = co_await ss.send(sent);
        if (!sr.has_value())
        {
            logc(ss, "Send failed on...  {}", sent);
             break;
        }
        logc(ss, "Sent...  {} bytes containing {}", sr.value(), sent);
        logc(ss, "Waiting for server to echo it");
        auto && rr = co_await ss.recv(buffer);
        logc(ss, "Received something...  {}");
        if (!rr.has_value())
        {
            logc(ss, "Receive failed when expecting...  {}", sent);
            break;
        }
        auto received{to_string_view(buffer, rr.value())};
        logc(ss, "Received...  {}", received);
        if (sent != received)
            break;
    }
    logc(ss, "Stopping...");
}

awaitable_t < void > run_echo_client(ring_t & ring, ipaddressv4_t const & ip, ipport_t const & port)
{
    log("Creating...");
    auto ss = tcp_socket_t(ring);
    log("Connecting...");
    auto cs = co_await ss.connect(ip, port);
    if (cs == socket_t::connect_status_t::SUCCEEDED)
        co_await handle_server(std::move(ss));
    log("Done with client...");
}

}

int main()
{
    ring_t ring{};
    run_echo_client(ring, IPADDRV4_LOOPBACK, ipport_t{56789});
    ring.run();
    return 0;
}
