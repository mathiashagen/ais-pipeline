#include "ais/static_data_report.hpp"

namespace ais {

std::optional<StaticDataPartA> decode_static_data_part_a(const Payload& payload) {
    if (payload.bit_count() < 160) {
        return std::nullopt;
    }
    const auto type = payload.get_uint(0, 6);
    const auto part = payload.get_uint(38, 2);
    if (type != 24 || part != 0) {
        return std::nullopt;
    }

    StaticDataPartA data;
    data.mmsi = static_cast<std::uint32_t>(payload.get_uint(8, 30));
    data.name = payload.get_text(40, 120);
    return data;
}

std::optional<StaticDataPartB> decode_static_data_part_b(const Payload& payload) {
    if (payload.bit_count() < 162) {
        return std::nullopt;
    }
    const auto type = payload.get_uint(0, 6);
    const auto part = payload.get_uint(38, 2);
    if (type != 24 || part != 1) {
        return std::nullopt;
    }

    StaticDataPartB data;
    data.mmsi = static_cast<std::uint32_t>(payload.get_uint(8, 30));
    data.ship_type = static_cast<std::uint8_t>(payload.get_uint(40, 8));
    data.call_sign = payload.get_text(90, 42);
    data.to_bow = static_cast<std::uint16_t>(payload.get_uint(132, 9));
    data.to_stern = static_cast<std::uint16_t>(payload.get_uint(141, 9));
    data.to_port = static_cast<std::uint8_t>(payload.get_uint(150, 6));
    data.to_starboard = static_cast<std::uint8_t>(payload.get_uint(156, 6));
    return data;
}

}