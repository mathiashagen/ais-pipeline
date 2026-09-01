#include "httplib.h"
#include "ais/position_reader.hpp"

int main() {
    httplib::Server server;

    ais::PositionReader reader("ais_data.db");

    server.Get("/positions", [&reader](const httplib::Request&, httplib::Response& res) {
        auto records = reader.latest_positions();
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

        auto records = reader.positions_in_area(box);
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

        auto records = reader.history(mmsi, limit);
        nlohmann::json j = records;
        res.set_content(j.dump(), "application/json");
    });

    server.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    server.listen("0.0.0.0", 8080);
}