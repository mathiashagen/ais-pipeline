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

std::optional<std::string_view> tag_block_source(std::string_view line) {
    if (line.empty() || line.front() != '\\') {
        return std::nullopt;
    }
    auto end_pos = line.find('\\', 1);
    if (end_pos == std::string_view::npos) {
        return std::nullopt;
    }

    // Take the contents between them and cut off *hh   
    auto tag_block = line.substr(1, end_pos - 1);
    auto asterisk_pos = tag_block.find('*');
    if (asterisk_pos != std::string_view::npos) {
        tag_block = tag_block.substr(0, asterisk_pos);
    }

    // go through the comma-seperated fields and find the one that starts with "s:"
    auto fields = tag_block;
    std::size_t start = 0;
    while (start < fields.size()) {
        auto comma_pos = fields.find(',', start);
        auto field = (comma_pos == std::string_view::npos) ? fields.substr(start) : fields.substr(start, comma_pos - start);
        if (field.size() > 2 && field.starts_with("s:")) {
            return field.substr(2);
        }
        if (comma_pos == std::string_view::npos) {
            break;
        }
        start = comma_pos + 1;
    }

    return std::nullopt;
}
}