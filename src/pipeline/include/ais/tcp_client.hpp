#pragma once
#include <string>
#include "ais/concurrent_queue.hpp"
#include <stop_token>

namespace ais {

class TcpClient {
public:
    TcpClient(std::string host, std::string port);
    void run(ThreadSafeQueue<std::string>& output, std::stop_token stop_token);
private:
    std::string host_;
    std::string port_;
};
}