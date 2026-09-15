#pragma once

#include "ais/payload.hpp"
#include <optional>
#include <cstdint>

namespace ais {

enum class NavStatus : std::uint8_t {
    UnderWayEngine       = 0,
    AtAnchor              = 1,
    NotUnderCommand       = 2,
    RestrictedManeuver    = 3,
    ConstrainedByDraught  = 4,
    Moored                = 5,
    Aground               = 6,
    Fishing               = 7,
    UnderWaySailing       = 8,
    ReservedHSC           = 9,
    ReservedWIG           = 10,
    Reserved11            = 11,
    Reserved12            = 12,
    Reserved13            = 13,
    AisSartMobEpirb       = 14,
    NotDefined            = 15,
};

class PositionReport {
public:
    std::uint8_t message_type = 0;
    std::uint8_t repeat_indicator = 0;
    std::uint32_t mmsi = 0;
    NavStatus nav_status = NavStatus::NotDefined;
    std::optional<std::int8_t> rate_of_turn;
    std::optional<double> sog;
    bool position_accuracy = false;
    std::optional<double> longitude;
    std::optional<double> latitude;
    std::optional<double> cog;
    std::optional<int> true_heading;
    std::uint8_t timestamp = 0;
    std::uint8_t maneuver_indicator = 0;
    bool raim = false;
    std::uint32_t radio_status = 0;
};

std::optional<PositionReport> decode_position_report(const Payload& payload);
std::optional<PositionReport> decode_class_b_position_report(const Payload& payload);

}  // namespace ais