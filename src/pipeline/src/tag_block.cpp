#include "ais/tag_block.hpp"

namespace ais {
    std::optional<std::string_view> strip_tag_block(std::string_view line) {
        if (line.empty() || line.front() != '\\') {
            return line;
        }
        auto end_pos = line.find('\\', 1);
        if (end_pos == std::string_view::npos) {
            return std::nullopt;
        }
        return line.substr(end_pos + 1);
    }
}