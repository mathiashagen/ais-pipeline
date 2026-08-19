#include "ais/decoder.hpp"
#include "ais/payload.hpp"

namespace ais {

std::optional<PositionReport> decode_sentence(const Sentence& sentence) {
    if (!sentence.is_valid() || !sentence.is_well_formed()) {
        return std::nullopt;
    }

    const std::string_view payload_str = sentence.payload();
    const int fill_bits = sentence.fill_bits();

    Payload payload(payload_str, fill_bits);

    return decode_position_report(payload);
}

}