#include "ais/position_report.hpp"

namespace ais {

std::optional<PositionReport> decode_position_report(const Payload& payload) {
    if (payload.bit_count() < 168) {
        return std::nullopt; // Not enough bits for a valid position report
    }
    const auto type = payload.get_uint(0, 6);
    if (type < 1 || type > 3) {
        return std::nullopt; // Not a type 1-3 position report
    }

    PositionReport report;
    report.message_type = static_cast<std::uint8_t>(type);
    report.mmsi = static_cast<std::uint32_t>(payload.get_uint(8, 30));
    report.nav_status = static_cast<NavStatus>(payload.get_uint(38, 4));

    const auto sog_raw = payload.get_uint(50, 10);
    if (sog_raw != 1023) {
        report.sog = static_cast<double>(sog_raw) / 10.0; // Convert to knots
    }

    report.position_accuracy = payload.get_uint(60, 1) == 1;
    
    const auto lon_raw = payload.get_int(61, 28);
    if (lon_raw != 108600000) {
        report.longitude = static_cast<double>(lon_raw) / 600000.0;
    }

    const auto lat_raw = payload.get_int(89, 27);
    if (lat_raw != 54600000) {
        report.latitude = static_cast<double>(lat_raw) / 600000.0;
    }

    const auto cog_raw = payload.get_uint(116, 12);
    if (cog_raw != 3600) {
        report.cog = static_cast<double>(cog_raw) / 10.0;
    }

    const auto true_heading_raw = payload.get_uint(128, 9);
    if (true_heading_raw != 511) {
        report.true_heading = static_cast<int>(true_heading_raw);
    }

    const auto rot_raw = payload.get_int(42, 8);
    if (rot_raw != -128) {
        report.rate_of_turn = static_cast<std::int8_t>(rot_raw);
    }

    report.timestamp = static_cast<std::uint8_t>(payload.get_uint(137, 6));
    report.maneuver_indicator = static_cast<std::uint8_t>(payload.get_uint(143, 2));
    report.raim = payload.get_uint(148, 1) == 1;
    report.radio_status = static_cast<std::uint32_t>(payload.get_uint(149, 19));

    return report;
}

std::optional<PositionReport> decode_class_b_position_report(const Payload& payload) {
    if (payload.bit_count() < 168) {
        return std::nullopt; // Not enough bits for a valid class B position report
    }
    const auto type = payload.get_uint(0, 6);
    if (type != 18) {
        return std::nullopt; // Not a class B position report
    }

    PositionReport report;
    report.message_type = static_cast<std::uint8_t>(type);
    report.mmsi = static_cast<std::uint32_t>(payload.get_uint(8, 30));

    const auto sog_raw = payload.get_uint(46, 10);
    if (sog_raw != 1023) {
        report.sog = static_cast<double>(sog_raw) / 10.0; // Convert to knots
    }

    report.position_accuracy = payload.get_uint(56, 1) == 1;
    
    const auto lon_raw = payload.get_int(57, 28);
    if (lon_raw != 108600000) {
        report.longitude = static_cast<double>(lon_raw) / 600000.0;
    }

    const auto lat_raw = payload.get_int(85, 27);
    if (lat_raw != 54600000) {
        report.latitude = static_cast<double>(lat_raw) / 600000.0;
    }

    const auto cog_raw = payload.get_uint(112, 12);
    if (cog_raw != 3600) {
        report.cog = static_cast<double>(cog_raw) / 10.0;
    }

    const auto true_heading_raw = payload.get_uint(124, 9);
    if (true_heading_raw != 511) {
        report.true_heading = static_cast<int>(true_heading_raw);
    }

    report.timestamp = static_cast<std::uint8_t>(payload.get_uint(133, 6));

    report.raim = payload.get_uint(147, 1) == 1;
    report.radio_status = static_cast<std::uint32_t>(payload.get_uint(148, 20));

    return report;
}

}  // namespace ais