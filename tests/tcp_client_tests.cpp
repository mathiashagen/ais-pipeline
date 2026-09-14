#include "ais/line_framer.hpp"
#include "ais/tcp_client.hpp"
#include "ais/concurrent_queue.hpp"

#include <gtest/gtest.h>
#include <asio.hpp>

#include <chrono>
#include <exception>
#include <string>
#include <system_error>
#include <thread>

namespace {

using namespace std::chrono_literals;

// A one-connection TCP server on 127.0.0.1, so TcpClient can be tested without
// the network and without depending on Kystverket's feed being up. It sends
// `payload` to the first client, then either keeps the connection open and
// silent or closes it.
class LocalFeed {
public:
    enum class AfterSending { StayOpen, Close };

    explicit LocalFeed(std::string payload = "", AfterSending after = AfterSending::StayOpen)
        : payload_(std::move(payload)), after_(after) {
        port_ = std::to_string(acceptor_.local_endpoint().port());

        acceptor_.async_accept(peer_, [this](std::error_code error) {
            if (error) return;
            asio::async_write(peer_, asio::buffer(payload_), [this](std::error_code, std::size_t) {
                if (after_ == AfterSending::Close) {
                    std::error_code ignored;
                    peer_.close(ignored);
                }
            });
        });

        thread_ = std::thread([this] {
            // Keeps run() going after the accept and write are done, so a
            // StayOpen connection stays open until the destructor stops it.
            asio::executor_work_guard<asio::io_context::executor_type> work(io_.get_executor());
            io_.run();
        });
    }

    ~LocalFeed() {
        io_.stop();  // safe from another thread, unlike touching the sockets
        thread_.join();
    }

    LocalFeed(const LocalFeed&) = delete;
    LocalFeed& operator=(const LocalFeed&) = delete;

    const std::string& port() const { return port_; }

private:
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_{io_, {asio::ip::make_address("127.0.0.1"), 0}};  // 0: any free port
    asio::ip::tcp::socket peer_{io_};
    std::string payload_;
    AfterSending after_;
    std::string port_;
    std::thread thread_;  // last, so it starts after everything it uses exists
};

}  // namespace

TEST(TcpClient, PushesLinesFromFeed) {
    LocalFeed feed("!AIVDM,first\r\n!AIVDM,second\r\n");
    ais::TcpClient client("127.0.0.1", feed.port(), 5s, 5s);
    ais::ThreadSafeQueue<std::string> lines(10);

    std::exception_ptr failure;
    std::jthread client_thread([&](std::stop_token stop_token) {
        try {
            client.run(lines, stop_token);
        } catch (...) {
            failure = std::current_exception();
        }
    });

    EXPECT_EQ(lines.pop(), "!AIVDM,first");
    EXPECT_EQ(lines.pop(), "!AIVDM,second");

    client_thread.request_stop();
    client_thread.join();
    EXPECT_FALSE(failure) << "a requested stop must end run() without an exception";
}

// The bug this guards against: a connection that stays open but sends nothing
// used to block read_some forever, so the pipeline never reconnected.
TEST(TcpClient, ThrowsWhenTheFeedGoesSilent) {
    LocalFeed silent_feed;
    ais::TcpClient client("127.0.0.1", silent_feed.port(), 5s, 200ms);
    ais::ThreadSafeQueue<std::string> lines(10);
    std::stop_source never_stopped;

    // Specifically the timeout error: std::system_error, thrown for a dropped
    // connection, also derives from std::runtime_error and would pass a
    // looser check without the timeout ever firing.
    const auto start = std::chrono::steady_clock::now();
    EXPECT_THROW(client.run(lines, never_stopped.get_token()), ais::FeedTimeoutError);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 3s);
}

// Ctrl+C must not wait out the read timeout: stopping ends a silent read at once.
TEST(TcpClient, StopEndsASilentReadPromptly) {
    LocalFeed silent_feed;
    ais::TcpClient client("127.0.0.1", silent_feed.port(), 5s, 60s);
    ais::ThreadSafeQueue<std::string> lines(10);

    std::exception_ptr failure;
    std::jthread client_thread([&](std::stop_token stop_token) {
        try {
            client.run(lines, stop_token);
        } catch (...) {
            failure = std::current_exception();
        }
    });

    std::this_thread::sleep_for(200ms);  // let it connect and start reading

    const auto stop_requested_at = std::chrono::steady_clock::now();
    client_thread.request_stop();
    client_thread.join();

    EXPECT_LT(std::chrono::steady_clock::now() - stop_requested_at, 1s);
    EXPECT_FALSE(failure) << "a requested stop must end run() without an exception";
}

TEST(TcpClient, ThrowsWhenTheFeedClosesTheConnection) {
    LocalFeed closing_feed("!AIVDM,last\r\n", LocalFeed::AfterSending::Close);
    ais::TcpClient client("127.0.0.1", closing_feed.port(), 5s, 5s);
    ais::ThreadSafeQueue<std::string> lines(10);
    std::stop_source never_stopped;

    EXPECT_THROW(client.run(lines, never_stopped.get_token()), std::system_error);
    // The line that arrived before the close was still delivered.
    EXPECT_EQ(lines.pop(), "!AIVDM,last");
}

// The two tests below talk to Kystverket's live feed. They say more about the
// feed than about this code -- it can be silent for minutes -- so they are
// disabled and do not run in CI. To run them deliberately:
//   all_tests --gtest_also_run_disabled_tests --gtest_filter='TcpClient.DISABLED_*'

TEST(TcpClient, DISABLED_ConnectsToLiveFeed) {
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

TEST(TcpClient, DISABLED_RunPushesLinesFromLiveFeed) {
    // A bounded read timeout, so a silent feed fails this test instead of hanging it.
    ais::TcpClient client("153.44.253.27", "5631", 10s, 60s);
    ais::ThreadSafeQueue<std::string> queue(100);
    std::jthread client_thread([&client, &queue](std::stop_token stop_token) {
        try {
            client.run(queue, stop_token);
        } catch (const std::exception&) {
            queue.close();  // lets the pops below return instead of blocking
        }
    });
    auto item = queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_FALSE(item.value().empty());
    auto item2 = queue.pop();
    ASSERT_TRUE(item2.has_value());
    EXPECT_FALSE(item2.value().empty());
    client_thread.request_stop();
    queue.close();
}
