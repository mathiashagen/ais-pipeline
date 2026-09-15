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
    const FragmentKey key{std::string(sentence.channel()), *sentence.sequence_id()};

    auto& group = pending_[key];

    if (sentence.fragment_number() == 1) {
        group.clear();
        group.push_back(sentence);
        return std::nullopt;
    }

    if (static_cast<std::size_t>(sentence.fragment_number()) != group.size() + 1) {
        pending_.erase(key);
        return std::nullopt;
    }

    group.push_back(sentence);

    if (group.size() == static_cast<std::size_t>(sentence.fragment_count())) {
        std::string combined_payload;
        int fill_bits = 0;

        for (const auto& s : group) {
            combined_payload += s.payload();
            if (s.fragment_number() == s.fragment_count()) {
                fill_bits = s.fill_bits();
            }
        }

        pending_.erase(key);
        return Payload(combined_payload, fill_bits);
    }

    return std::nullopt;
}

}