#pragma once
#include <chrono>
#include <stdexcept>
#include <string>
#include "ais/concurrent_queue.hpp"
#include <stop_token>

namespace ais {

// Thrown when the feed does not connect, or sends nothing, within its timeout.
// A type of its own so callers and tests can tell "silent" from "disconnected",
// which surfaces as std::system_error.
class FeedTimeoutError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class TcpClient {
public:
    static constexpr std::chrono::seconds default_connect_timeout{10};

    // Kystverket's feed has been seen silent for over eight minutes on a live
    // connection before delivering a burst on that same connection. Much
    // shorter, and the pipeline would drop a working connection just before
    // its data arrived, and keep doing it.
    static constexpr std::chrono::minutes default_read_timeout{10};

    TcpClient(std::string host, std::string port,
              std::chrono::milliseconds connect_timeout = default_connect_timeout,
              std::chrono::milliseconds read_timeout = default_read_timeout);

    // Connects and pushes each complete line into `output` until one of:
    //  - `stop_token` is stopped: returns normally, promptly, even mid-read;
    //  - connecting or a read times out: throws FeedTimeoutError;
    //  - the connection fails or is closed: throws std::system_error.
    // The caller reconnects by calling run() again.
    void run(ThreadSafeQueue<std::string>& output, std::stop_token stop_token);

private:
    std::string host_;
    std::string port_;
    std::chrono::milliseconds connect_timeout_;
    std::chrono::milliseconds read_timeout_;
};
}
