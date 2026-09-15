#include "ais/ais_message.hpp"

namespace ais {

std::optional<AisMessage> decode_message(const Payload& payload) {
    if (payload.bit_count() < 6) {
        return std::nullopt;
    }
    const auto type = payload.get_uint(0, 6);
    if (type > 0 && type < 4) {
        const auto report = decode_position_report(payload);
        if (report) {
            return *report;
        }
    }
    if (type == 5) {
        auto report = decode_static_voyage_data(payload);
        if (report) {
            return std::move(*report);
        }
    }
    return std::nullopt;
}

}