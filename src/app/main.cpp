#include <csignal>
#include <stop_token>
#include <thread>
#include <algorithm>

#include "ais/concurrent_queue.hpp"
#include "ais/position_report.hpp"
#include "ais/tcp_client.hpp"
#include "ais/decoder_stage.hpp"
#include "ais/sqlite_writer.hpp"

std::stop_source g_stop_source;

extern "C" void handle_sigint(int) {
    g_stop_source.request_stop();
}

int main() {
    std::signal(SIGINT, handle_sigint);

    ais::ThreadSafeQueue<std::string> queue1(500);
    ais::ThreadSafeQueue<ais::PositionReport> queue2(500);

    ais::TcpClient client("153.44.253.27", "5631");
    ais::DecoderStage decoder;
    ais::SqliteWriter writer("ais_data.db");

    std::jthread client_thread([&client, &queue1](std::stop_token st) {
        int fail_count = 0;
        while (!st.stop_requested()) {
            try {
                client.run(queue1, st);
                break;
            } catch (const std::exception&) {
                ++fail_count;

                std::chrono::milliseconds sleep_duration(std::min(1000 * (1 << fail_count), 30000));
                auto start = std::chrono::steady_clock::now();
                while (!st.stop_requested()) {
                    auto now = std::chrono::steady_clock::now();
                    if (now - start >= sleep_duration) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }
    });

    std::jthread decoder_thread([&decoder, &queue1, &queue2](std::stop_token st) {
        decoder.run(queue1, queue2, st);
    });

    std::jthread writer_thread([&writer, &queue2](std::stop_token st) {
        writer.run(queue2, st);
    });

    while (!g_stop_source.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    client_thread.request_stop();
    client_thread.join();

    queue1.close();
    decoder_thread.join();

    queue2.close();
    writer_thread.join();

    return 0;
}