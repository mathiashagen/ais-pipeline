#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "httplib.h"
#include "ais/position_reader.hpp"

namespace {

// api_server [db_path] [port]. Defaults match kystverket_pipeline's, so the
// two find the same database when started from the same directory.
struct Config {
    std::string db_path = "ais_data.db";
    int port = 8080;
};

void print_usage(std::string_view program) {
    std::cerr << "usage: " << program << " [db_path] [port]\n"
              << "  db_path  SQLite database written by kystverket_pipeline (default ais_data.db)\n"
              << "  port     HTTP port to listen on                          (default 8080)\n";
}

}  // namespace

// Same pattern as kystverket_pipeline: the handler only sets a lock-free
// atomic, and ordinary code in main does the actual shutdown.
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
    if (argc > 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    Config config;
    if (argc > 1) config.db_path = argv[1];
    if (argc > 2) {
        try {
            config.port = std::stoi(argv[2]);
        } catch (const std::exception&) {
            std::cerr << "port must be a number\n";
            return EXIT_FAILURE;
        }
    }

    // The reader opens the database read-only, so a missing file is an error
    // here rather than something SQLite silently creates. Report it instead
    // of letting the exception terminate the process.
    std::optional<ais::PositionReader> reader;
    try {
        reader.emplace(config.db_path);
    } catch (const std::exception& e) {
        std::cerr << "Could not open " << config.db_path << ": " << e.what()
                  << "\nRun kystverket_pipeline first, or pass the path to its database.\n";
        return EXIT_FAILURE;
    }

    httplib::Server server;

    server.Get("/positions", [&reader](const httplib::Request&, httplib::Response& res) {
        auto records = reader->latest_positions();
        nlohmann::json j = records;

        res.set_content(j.dump(), "application/json");
    });

    server.Get("/positions/area", [&reader](const httplib::Request& req, httplib::Response& res) {
        // Only parsing is guarded: a query that throws is the server's fault,
        // not the client's, and must not be reported as a 400. Letting it
        // escape the handler gives httplib's default 500.
        ais::BoundingBox box{};
        try {
            box = ais::BoundingBox{
                std::stod(req.get_param_value("min_lat")),
                std::stod(req.get_param_value("max_lat")),
                std::stod(req.get_param_value("min_lon")),
                std::stod(req.get_param_value("max_lon"))
            };
        } catch (const std::exception&) {
            res.status = 400;
            res.set_content("invalid or missing query parameters", "text/plain");
            return;
        }

        auto records = reader->positions_in_area(box);
        nlohmann::json j = records;
        res.set_content(j.dump(), "application/json");
    });

    server.Get(R"(/positions/(\d+)/history)", [&reader](const httplib::Request& req, httplib::Response& res) {
        std::uint32_t mmsi = 0;
        int limit = 50;
        try {
            mmsi = std::stoul(req.matches[1]);
            const std::string limit_param = req.get_param_value("limit");
            if (!limit_param.empty()) {
                limit = std::stoi(limit_param);
            }
        } catch (const std::exception&) {
            res.status = 400;
            res.set_content("invalid or missing query parameters", "text/plain");
            return;
        }

        // SQLite reads a negative LIMIT as "no limit", so ?limit=-1 would
        // return a ship's entire history.
        if (limit < 1 || limit > 1000) {
            res.status = 400;
            res.set_content("limit must be between 1 and 1000", "text/plain");
            return;
        }

        auto records = reader->history(mmsi, limit);
        nlohmann::json j = records;
        res.set_content(j.dump(), "application/json");
    });

    server.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);

    // listen() would block this thread until stop(), and stop() must not be
    // called from the signal handler. So bind here, where a failure (port in
    // use, no permission) is reported directly, serve on a second thread, and
    // let this thread wait for the flag and call stop().
    if (!server.bind_to_port("0.0.0.0", config.port)) {
        std::cerr << "Could not listen on port " << config.port << "\n";
        return EXIT_FAILURE;
    }

    std::cerr << "Serving " << config.db_path << " on http://localhost:" << config.port << "\n";

    std::atomic<bool> server_returned{false};
    std::jthread server_thread([&server, &server_returned] {
        server.listen_after_bind();
        server_returned.store(true);
    });

    while (!g_stop_requested.load() && !server_returned.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // stop() closes the listening socket; listen_after_bind() then lets the
    // requests in progress finish before it returns. Safe to call even if the
    // server thread has not reached listen_after_bind() yet.
    server.stop();
    server_thread.join();

    if (!g_stop_requested.load()) {
        std::cerr << "Server stopped unexpectedly\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
