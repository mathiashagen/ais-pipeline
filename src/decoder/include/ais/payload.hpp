#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ais {

// The decoded AIS payload: the 6-bit-armored characters of field 6 turned into
// a flat bitstream, with the trailing fill bits removed. Lets you read unsigned
// integer fields out of the message by their bit position.
//
// Storage (option 1): one std::uint8_t per bit, each 0 or 1. Wasteful but the
// bit-reading code stays obvious. A message is only a few hundred bits.
class Payload {
public:
    // armored   = field 6 (the 6-bit ASCII characters)
    // fill_bits = field 7 (0..5): how many bits to drop from the very end
    Payload(std::string_view armored, int fill_bits);

    // Number of usable bits (after dropping fill bits).
    std::size_t bit_count() const { return bits_.size(); }

    // Reads `length` bits starting at bit index `start`, most-significant bit
    // first, as an unsigned integer. AIS integer fields are big-endian.
    std::uint64_t get_uint(std::size_t start, std::size_t length) const;

    std::int64_t get_int(std::size_t start, std::size_t length) const;

private:
    std::vector<std::uint8_t> bits_;  // one entry per bit, value 0 or 1
};

}  // namespace ais
