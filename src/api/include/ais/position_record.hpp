#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>
#include "ais/position_report.hpp"

namespace ais {

struct PositionRecord
{
    PositionReport report;
    std::int64_t received_at;
    std::optional<std::string> name;
    std::optional<std::string> call_sign;
    std::optional<std::string> destination;
    std::optional<int> ship_type;
};

inline void to_json(nlohmann::json& j, const PositionRecord& record) 
{
    j = nlohmann::json{
        {"mmsi", record.report.mmsi},
        {"latitude", record.report.latitude},
        {"longitude", record.report.longitude},
        {"timestamp", record.report.timestamp},
        {"nav_status", record.report.nav_status},
        {"sog", record.report.sog},
        {"cog", record.report.cog},
        {"true_heading", record.report.true_heading},
        {"received_at", record.received_at},
        {"name", record.name},
        {"call_sign", record.call_sign},
        {"destination", record.destination},
        {"ship_type", record.ship_type}
    };
};

} // namespace ais