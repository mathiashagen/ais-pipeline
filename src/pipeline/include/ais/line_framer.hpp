#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace ais {

class LineFramer {
public:
    std::vector<std::string> feed(std::string_view chunk);

private:
    std::string buffer_;
};
}