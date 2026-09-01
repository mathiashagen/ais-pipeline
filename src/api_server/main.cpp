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
        try {
            ais::BoundingBox box{
                std::stod(req.get_param_value("min_lat")),
                std::stod(req.get_param_value("max_lat")),
                std::stod(req.get_param_value("min_lon")),
                std::stod(req.get_param_value("max_lon"))
            };
            auto records = reader.positions_in_area(box);   
            nlohmann::json j = records;
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception&) {
            res.status = 400;
            res.set_content("invalid or missing query parameters", "text/plain");
        }
    });

    server.Get(R"(/positions/(\d+)/history)", [&reader](const httplib::Request& req, httplib::Response& res) {
        try {
            std::uint32_t mmsi = std::stoul(req.matches[1]);
            std::string limit_param = req.get_param_value("limit");
            int limit = limit_param.empty() ? 50 : std::stoi(limit_param);
            auto records = reader.history(mmsi, limit);
            nlohmann::json j = records;
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception&) {
            res.status = 400;
            res.set_content("invalid or missing query parameters", "text/plain");
        }
    });

    server.listen("0.0.0.0", 8080);
}