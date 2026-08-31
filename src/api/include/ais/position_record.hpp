#pragma once
#include <cstdint>
#include "ais/position_report.hpp"

namespace ais {

struct PositionRecord
{
    PositionReport report;
    std::int64_t received_at;
};
}