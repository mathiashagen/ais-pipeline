#pragma once

#include <optional>
#include <string_view>

namespace ais {

std::optional<std::string_view> strip_tag_block(std::string_view line);

std::optional<std::string_view> tag_block_source(std::string_view line);

}