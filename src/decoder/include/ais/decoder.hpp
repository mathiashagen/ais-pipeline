#pragma once

#include <optional>

#include "ais/sentence.hpp"
#include "ais/position_report.hpp"

namespace ais {

std::optional<PositionReport> decode_sentence(const Sentence& sentence);

}