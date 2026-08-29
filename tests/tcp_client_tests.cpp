#include "ais/line_framer.hpp"
#include "ais/tcp_client.hpp"
#include "ais/concurrent_queue.hpp"

#include <gtest/gtest.h>
#include <asio.hpp>
#include <thread>

TEST(TcpClient, ConnectsToLiveFeed) {
    asio::io_context io_context;
    asio::ip::tcp::resolver resolver(io_context);
    asio::ip::tcp::socket socket(io_context);

    asio::connect(socket, resolver.resolve("153.44.253.27", "5631"));
    ASSERT_TRUE(socket.is_open());

    std::array<char, 4096> socket_data;
    std::string_view chunk = std::string_view(socket_data.data(), socket.read_some(asio::buffer(socket_data)));

    ais::LineFramer framer;
    auto lines = framer.feed(chunk);
    ASSERT_FALSE(lines.empty());
    for (const auto& line : lines) {
        ASSERT_FALSE(line.empty());
    }
}

TEST(TcpClient, RunPushesLinesFromLiveFeed) {
    ais::TcpClient client("153.44.253.27", "5631");
    ais::ThreadSafeQueue<std::string> queue(100);
    std::jthread client_thread([&client, &queue](std::stop_token stop_token) {
        try {
            client.run(queue, stop_token);
        } catch (const std::exception&) {
            // expected once we close() the queue below to force shutdown
        }
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto item = queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_FALSE(item.value().empty());
    auto item2 = queue.pop();
    ASSERT_TRUE(item2.has_value());
    EXPECT_FALSE(item2.value().empty());
    client_thread.request_stop();
    queue.close();
}