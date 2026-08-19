#pragma once

#include <optional>
#include <map>
#include <vector>
#include "ais/payload.hpp"
#include "ais/sentence.hpp"

namespace ais {
class SentenceAssembler {
public:
    std::optional<Payload> add(const Sentence& sentence);

private:
    std::map<int, std::vector<Sentence>> pending_;
};
}