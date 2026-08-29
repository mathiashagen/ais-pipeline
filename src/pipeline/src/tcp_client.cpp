#include "ais/tcp_client.hpp"
#include "ais/line_framer.hpp"
#include <asio.hpp>

namespace ais {
TcpClient::TcpClient(std::string host, std::string port)
    : host_(std::move(host)), port_(std::move(port)) {}

void TcpClient::run(ThreadSafeQueue<std::string>& output, std::stop_token stop_token) {
    asio::io_context io_context;
    asio::ip::tcp::resolver resolver(io_context);
    asio::ip::tcp::socket socket(io_context);
    asio::connect(socket, resolver.resolve(host_, port_));

    ais::LineFramer framer;

    while (!stop_token.stop_requested()) {
        std::array<char, 4096> socket_data;
        std::string_view chunk = std::string_view(socket_data.data(), socket.read_some(asio::buffer(socket_data)));
        auto lines = framer.feed(chunk);
        for (auto& line : lines) {
            output.push(std::move(line));
        }
    }
}
}