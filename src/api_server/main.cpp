#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

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

    std::cerr << "Serving " << config.db_path << " on http://localhost:" << config.port << "\n";

    // listen() only returns on failure to bind (port in use, no permission)
    // or after stop(); nobody calls stop() here, so a return is an error.
    if (!server.listen("0.0.0.0", config.port)) {
        std::cerr << "Could not listen on port " << config.port << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
