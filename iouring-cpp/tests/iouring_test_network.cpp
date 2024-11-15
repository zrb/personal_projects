#include <iouring.hpp>

#include <catch2/catch_all.hpp>

#include <iostream>

using namespace zsl::iouring;
using namespace zsl::iouring::coroutine;
using namespace zsl::iouring::net;
using namespace zsl::iouring::scheduler;

using namespace zsl::logging;

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
            logc(ss, "<<<<<<<client>>>>>>> Send failed on...  {}", sent);
             break;
        }
        logc(ss, "<<<<<<<client>>>>>>> Sent...  {} bytes containing {}", sr.value(), sent);
        logc(ss, "<<<<<<<client>>>>>>> Waiting for server to echo it");
        auto && rr = co_await ss.recv(buffer);
        logc(ss, "<<<<<<<client>>>>>>> Received something...  {}");
        if (!rr.has_value())
        {
            logc(ss, "<<<<<<<client>>>>>>> Receive failed when expecting...  {}", sent);
            break;
        }
        auto received{to_string_view(buffer, rr.value())};
        logc(ss, "<<<<<<<client>>>>>>> Received...  {}", received);
        if (sent != received)
            break;
    }
    logc(ss, "<<<<<<<client>>>>>>> Stopping...");
}

awaitable_t < void > run_client(ring_t & ring, ipaddressv4_t const & ip, ipport_t const & port)
{
    log("<<<<<<<client>>>>>>> Creating...");
    auto ss = tcp_socket_t(ring);
    log("<<<<<<<client>>>>>>> Connecting...");
    auto cs = co_await ss.connect(ip, port);
    if (cs == socket_t::connect_status_t::SUCCEEDED)
        co_await handle_server(std::move(ss));
    log("<<<<<<<client>>>>>>> Done with client...");
}

awaitable_t < void > handle_client(tcp_socket_t cs)
{
    while (true)
    {
        logc(cs, "<<<<<<<server>>>>>>> Waiting for client to send something on... {}", cs);
        std::array < uint8_t, 4096 > buf{};
        socket_t::recv_result_t rr = co_await cs.recv(buf);
        if (!rr.has_value())
        {
            logc(cs, "<<<<<<<server>>>>>>> Client closed", rr.error());
            break;
        }
        
        auto data = to_string_view(buf, rr.value());
        logc(cs, "<<<<<<<server>>>>>>> Received... {} bytes containing {}", rr.value(), data);

        socket_t::send_result_t sr = co_await cs.send(data);

        if (!sr.has_value())
        {
            logc(cs, "<<<<<<<server>>>>>>> Client closed", sr.error());
            break;
        }

        logc(cs, "<<<<<<<server>>>>>>> Sent... {} bytes containing {}", sr.value(), data);
    }
    co_return;
}

awaitable_t < void > run_server(ring_t & ring, ipaddressv4_t const & ip, ipport_t const & port)
{
    auto s = tcp_server(ring, ip, port);
    zsl::logging::logc(s, "<<<<<<<server>>>>>>> Accepting...");
    auto && ar = co_await s.acceptor().accept();
    zsl::logging::logc(s, "<<<<<<<server>>>>>>> Accept completed...");
    if (ar.has_value())
    {
        auto cs = std::move(ar.value());
        zsl::logging::logc(s, "<<<<<<<server>>>>>>> Accept succeeded...  got client socket... {}", cs);
        co_await handle_client(std::move(cs));
    }
    else
    {
        zsl::logging::logc(s, "<<<<<<<server>>>>>>> Accept failed... {}", ar.error());
    }
}

awaitable_t < void > test_run_server_and_client(ring_t & ring, ipaddressv4_t const & ip, ipport_t const & port, bool & stopped)
{
    log("-------------------------------------------");
    auto s = run_server(ring, ip, port);
    log("-------------------------------------------");
    auto c = run_client(ring, ip, port);
    co_await c;
    co_await s;
    stopped = true;
}

TEST_CASE("iouring network tests", "iouring network tests")
{
    ring_t ring;
    SECTION("net/tcp/server")
    {
        log("Running test...  net/tcp/server");
        bool stopped{false};
        ipaddressv4_t ip{IPADDRV4_LOOPBACK};
        ipport_t port{56789};
        test_run_server_and_client(ring, ip, port, stopped);
        ring.run(stopped);
    }
}
