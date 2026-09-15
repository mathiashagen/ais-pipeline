#include "ais/static_voyage_data.hpp"

namespace ais {

std::optional<StaticVoyageData> decode_static_voyage_data(const Payload& payload) {
    if (payload.bit_count() < 424) {
        return std::nullopt;
    }
    const auto type = payload.get_uint(0, 6);
    if (type != 5) {
        return std::nullopt;
    }

    StaticVoyageData data;
    data.mmsi = static_cast<std::uint32_t>(payload.get_uint(8, 30));
    
    const auto imo = payload.get_uint(40, 30);
    if (imo != 0) {
        data.imo = static_cast<std::uint32_t>(imo);
    }

    data.call_sign = payload.get_text(70, 42);
    data.name = payload.get_text(112, 120);
    data.ship_type = static_cast<std::uint8_t>(payload.get_uint(232, 8));
    data.to_bow = static_cast<std::uint16_t>(payload.get_uint(240, 9));
    data.to_stern = static_cast<std::uint16_t>(payload.get_uint(249, 9));
    data.to_port = static_cast<std::uint8_t>(payload.get_uint(258, 6));
    data.to_starboard = static_cast<std::uint8_t>(payload.get_uint(264, 6));

    const auto eta_month = payload.get_uint(274, 4);
    if (eta_month != 0) {
        data.eta_month = static_cast<std::uint8_t>(eta_month);
    }
    const auto eta_day = payload.get_uint(278, 5);
    if (eta_day != 0) {
        data.eta_day = static_cast<std::uint8_t>(eta_day);
    }

    const auto eta_hour = payload.get_uint(283, 5);
    if (eta_hour != 24) {
        data.eta_hour = static_cast<std::uint8_t>(eta_hour);
    }

    const auto eta_minute = payload.get_uint(288, 6);
    if (eta_minute != 60) {
        data.eta_minute = static_cast<std::uint8_t>(eta_minute);
    }

    const auto draught = payload.get_uint(294, 8);
    if (draught != 0) {
        data.draught = static_cast<double>(draught) / 10.0;
    }

    data.destination = payload.get_text(302, 120);

    return data;

}

}