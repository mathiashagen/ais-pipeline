#include "ais/sentence_assembler.hpp"

#include <string>

namespace ais {

std::optional<Payload> SentenceAssembler::add(const Sentence& sentence) {
    if (sentence.fragment_count() == 1) {
        return Payload(sentence.payload(), sentence.fill_bits());
    }

    if (!sentence.sequence_id().has_value()) {
        return std::nullopt;
    }
    const int sequence_id = sentence.sequence_id().value();

    pending_[sequence_id].push_back(sentence);

    if (pending_[sequence_id].size() == sentence.fragment_count()) {
        std::string combined_payload;
        int fill_bits = 0;

        for (const auto& s : pending_[sequence_id]) {
            combined_payload += s.payload();
            if (s.fragment_number() == s.fragment_count()) {
                fill_bits = s.fill_bits();
            }
        }

        pending_.erase(sequence_id);
        return Payload(combined_payload, fill_bits);
    }

    return std::nullopt;
}

}