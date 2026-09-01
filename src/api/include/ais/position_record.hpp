#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include "ais/position_report.hpp"

namespace ais {

struct PositionRecord
{
    PositionReport report;
    std::int64_t received_at;
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
        {"received_at", record.received_at}
    };
};

} // namespace ais