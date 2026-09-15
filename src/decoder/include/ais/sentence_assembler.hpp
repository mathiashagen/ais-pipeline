#pragma once

#include <optional>
#include <map>
#include <vector>
#include <string>
#include <string_view>
#include "ais/payload.hpp"
#include "ais/sentence.hpp"

namespace ais {
class SentenceAssembler {
public:
    std::optional<Payload> add(const Sentence& sentence, std::string_view source = {});

private:
    struct FragmentKey {
        std::string source;
        std::string channel;
        int sequence_id = 0;

        auto operator<=>(const FragmentKey&) const = default;
    };

    std::map<FragmentKey, std::vector<Sentence>> pending_;
};
}