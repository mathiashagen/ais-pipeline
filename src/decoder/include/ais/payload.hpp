#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>
#include <string>

namespace ais {

class Payload {
public:
    Payload(std::string_view armored, int fill_bits);

    std::size_t bit_count() const { return bits_.size(); }

    std::string get_text(std::size_t start, std::size_t length) const;

    std::uint64_t get_uint(std::size_t start, std::size_t length) const;

    std::int64_t get_int(std::size_t start, std::size_t length) const;

private:
    std::vector<std::uint8_t> bits_;
};

}  // namespace ais
