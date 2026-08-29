#pragma once

#include <optional>
#include <string_view>

namespace ais {

std::optional<std::string_view> strip_tag_block(std::string_view line);

}