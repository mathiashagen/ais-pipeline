#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <algorithm>

#include "ais/concurrent_queue.hpp"
#include "ais/ais_message.hpp"
#include "ais/tcp_client.hpp"
#include "ais/decoder_stage.hpp"
#include "ais/sqlite_writer.hpp"

namespace {

// Defaults point at Kystverket's open feed so the pipeline runs with no
// arguments, but every value can be overridden positionally without a
// rebuild: kystverket_pipeline [host] [port] [db_path]
struct Config {
    std::string host = "153.44.253.27";
    std::string port = "5631";
    std::string db_path = "ais_data.db";
};

void print_usage(std::string_view program) {
    std::cerr << "usage: " << program << " [host] [port] [db_path]\n"
              << "  host     AIS feed host        (default 153.44.253.27)\n"
              << "  port     AIS feed TCP port    (default 5631)\n"
              << "  db_path  SQLite database file (default ais_data.db)\n";
}

Config parse_args(int argc, char* argv[]) {
    Config config;
    if (argc > 1) config.host = argv[1];
    if (argc > 2) config.port = argv[2];
    if (argc > 3) config.db_path = argv[3];
    return config;
}

}  // namespace

// A signal handler can interrupt any thread mid-instruction, even inside
// malloc or while a mutex is held, so all it may safely do is write a
// lock-free atomic. The shutdown itself happens in main once it sees the flag.
// std::signal passes no context, which is why the flag has to be global.
static_assert(std::atomic<bool>::is_always_lock_free);
constinit std::atomic<bool> g_stop_requested{false};

// SIGINT is Ctrl+C; SIGTERM is what docker stop and systemctl stop send.
extern "C" void handle_stop_signal(int) {
    g_stop_requested.store(true);
}

int main(int argc, char* argv[]) {
    if (argc > 1 && (std::string_view(argv[1]) == "-h" || std::string_view(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }
    if (argc > 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const Config config = parse_args(argc, argv);

    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);

    ais::ThreadSafeQueue<std::string> queue1(500);
    ais::ThreadSafeQueue<ais::AisMessage> queue2(500);

    ais::TcpClient client(config.host, config.port);
    ais::DecoderStage decoder;
    ais::SqliteWriter writer(config.db_path);

    std::cerr << "Reading AIS from " << config.host << ":" << config.port
              << ", writing to " << config.db_path << ". Ctrl+C to stop.\n";

    std::jthread client_thread([&client, &queue1](std::stop_token st) {
        int fail_count = 0;
        while (!st.stop_requested()) {
            try {
                client.run(queue1, st);
                break;
            } catch (const std::exception& e) {
                ++fail_count;
                std::cerr << "Connection failed (" << e.what() << "), retrying\n";

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

    while (!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    client_thread.request_stop();
    client_thread.join();

    queue1.close();
    decoder_thread.join();

    queue2.close();
    writer_thread.join();

    return EXIT_SUCCESS;
}
