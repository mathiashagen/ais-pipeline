#pragma once

#include <variant>
#include <optional>
#include "ais/payload.hpp"
#include "ais/position_report.hpp"
#include "ais/static_voyage_data.hpp"
#include "ais/static_data_report.hpp"

namespace ais {

using AisMessage = std::variant<PositionReport, StaticVoyageData, StaticDataPartA, StaticDataPartB>;

std::optional<AisMessage> decode_message(const Payload& payload);

}