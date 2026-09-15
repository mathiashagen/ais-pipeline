#pragma once

#include "ais/payload.hpp"

#include <optional>
#include <string>
#include <cstdint>

namespace ais {

class StaticDataPartA {
public:
    std::uint32_t mmsi = 0;
    std::string name;
};

std::optional<StaticDataPartA> decode_static_data_part_a(const Payload& payload);

class StaticDataPartB {
public:
    std::uint32_t mmsi = 0;
    std::uint8_t ship_type = 0;
    std::string call_sign;
    std::uint16_t to_bow = 0;
    std::uint16_t to_stern = 0;
    std::uint8_t to_port = 0;
    std::uint8_t to_starboard = 0;
};

std::optional<StaticDataPartB> decode_static_data_part_b(const Payload& payload);

}