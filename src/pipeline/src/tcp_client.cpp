#include "ais/tcp_client.hpp"
#include "ais/line_framer.hpp"
#include <asio.hpp>

#include <array>
#include <string_view>
#include <system_error>

namespace ais {

namespace {

// Runs the io_context until its outstanding operation completes or `timeout`
// passes, and reports which happened.
//
// On timeout the socket is closed. That does not abandon the operation: it
// completes it, with operation_aborted, and the second run() delivers that
// completion so nothing is left pending when the next operation starts.
bool completed_within(asio::io_context& io, asio::ip::tcp::socket& socket,
                      std::chrono::milliseconds timeout) {
    io.restart();  // run_for leaves the context stopped once its work is done
    io.run_for(timeout);
    if (io.stopped()) {
        return true;  // the work finished: stopped means "nothing left to run"
    }

    std::error_code ignored;
    socket.close(ignored);
    io.run();
    return false;
}

}  // namespace

TcpClient::TcpClient(std::string host, std::string port,
                     std::chrono::milliseconds connect_timeout,
                     std::chrono::milliseconds read_timeout)
    : host_(std::move(host)),
      port_(std::move(port)),
      connect_timeout_(connect_timeout),
      read_timeout_(read_timeout) {}

// Blocking read_some had no way out: a connection that stayed open but sent
// nothing held the thread forever, so the pipeline never reconnected, and
// Ctrl+C waited on that same read. Each operation is now started
// asynchronously and the io_context is run for at most its timeout.
void TcpClient::run(ThreadSafeQueue<std::string>& output, std::stop_token stop_token) {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);

    // Runs on whichever thread requests the stop -- on Ctrl+C that is main,
    // while this thread is inside io.run_for(). Asio sockets must not be used
    // from two threads at once, but posting to an io_context is thread-safe:
    // the close runs here, on this thread, and ends the pending read.
    //
    // Declared after io and socket, so it is destroyed first; its destructor
    // waits for a callback already running on another thread, so neither can
    // be touched after they are gone.
    std::stop_callback close_on_stop(stop_token, [&io, &socket] {
        asio::post(io, [&socket] {
            std::error_code ignored;
            socket.close(ignored);
        });
    });

    // Blocking, but instant for the numeric address the pipeline uses; only
    // a host name would wait on DNS here.
    asio::ip::tcp::resolver resolver(io);
    const auto endpoints = resolver.resolve(host_, port_);

    std::error_code connect_error;
    asio::async_connect(socket, endpoints,
                        [&connect_error](const std::error_code& error, const asio::ip::tcp::endpoint&) {
                            connect_error = error;
                        });
    const bool connected_in_time = completed_within(io, socket, connect_timeout_);

    // Checked first: a stop also shows up as an aborted connect, and a stop is
    // not a failure.
    if (stop_token.stop_requested()) return;
    if (!connected_in_time) {
        throw FeedTimeoutError("no connection to " + host_ + ":" + port_ + " within " +
                               std::to_string(connect_timeout_.count()) + " ms");
    }
    if (connect_error) throw std::system_error(connect_error, "connect");

    LineFramer framer;
    std::array<char, 4096> buffer;

    for (;;) {
        std::error_code read_error;
        std::size_t bytes_read = 0;
        socket.async_read_some(asio::buffer(buffer),
                               [&read_error, &bytes_read](const std::error_code& error, std::size_t n) {
                                   read_error = error;
                                   bytes_read = n;
                               });
        const bool read_in_time = completed_within(io, socket, read_timeout_);

        if (!read_in_time || read_error) {
            if (stop_token.stop_requested()) return;
            if (!read_in_time) {
                throw FeedTimeoutError("no data from " + host_ + ":" + port_ + " for " +
                                       std::to_string(read_timeout_.count()) + " ms");
            }
            throw std::system_error(read_error, "read");  // includes the feed closing the connection
        }

        // Data that arrived is delivered even if a stop was requested meanwhile.
        for (auto& line : framer.feed(std::string_view(buffer.data(), bytes_read))) {
            output.push(std::move(line));
        }

        if (stop_token.stop_requested()) return;
    }
}
}
