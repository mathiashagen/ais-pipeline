#pragma once

#include "ais/payload.hpp"

#include <optional>
#include <string>
#include <cstdint>

namespace ais {

class StaticVoyageData {
    public:
        std::uint32_t mmsi = 0;
        std::optional<std::uint32_t> imo;
        std::string call_sign;
        std::string name;
        std::uint8_t ship_type = 0;
        std::uint16_t to_bow = 0;
        std::uint16_t to_stern = 0;
        std::uint8_t to_port = 0;
        std::uint8_t to_starboard = 0;
        std::optional<std::uint8_t> eta_month;
        std::optional<std::uint8_t> eta_day;
        std::optional<std::uint8_t> eta_hour;
        std::optional<std::uint8_t> eta_minute;
        std::optional<double> draught;
        std::string destination;
};

std::optional<StaticVoyageData> decode_static_voyage_data(const Payload& payload);

} // namespace ais