#include "ais/line_framer.hpp"

namespace ais {
std::vector<std::string> LineFramer::feed(std::string_view chunk) {
    buffer_ += chunk;
    std::vector<std::string> lines;
    size_t pos = 0;
    while ((pos = buffer_.find('\n')) != std::string::npos) {
        size_t line_end = pos;
        while (line_end > 0 && buffer_[line_end - 1] == '\r') {
            --line_end;
        }
        lines.push_back(buffer_.substr(0, line_end));
        buffer_.erase(0, pos + 1);
    }
    return lines;
}
}