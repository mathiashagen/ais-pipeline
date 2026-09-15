#include "ais/payload.hpp"
#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace ais {
namespace {
    constexpr std::string_view sixbit_text_alphabet = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_ !\"#$%&'()*+,-./0123456789:;<=>?";
    static_assert(sixbit_text_alphabet.size() == 64);
}

Payload::Payload(std::string_view armored, int fill_bits) {
    // 1. reserve space for armored.size() * 6 bits
    bits_.reserve(armored.size() * 6);

    // 2. for each char: decode to 0..63, then push its 6 bits MSB-first
    for (char c : armored) {
        int value = static_cast<unsigned char>(c) - 48;
        if (value > 40) value -= 8;
        for (int i = 5; i >= 0; --i) {
            bits_.push_back((value >> i) & 1);
        }
    }

    // 3. clamp fill_bits to a sane range, then shrink bits_ by that much
    if (fill_bits < 0) fill_bits = 0;
    if (fill_bits > 5) fill_bits = 5;
    const std::size_t drop = std::min(static_cast<std::size_t>(fill_bits), bits_.size());
    bits_.resize(bits_.size() - drop);
}

std::string Payload::get_text(std::size_t start, std::size_t length) const {
    if (length % 6 != 0) {
        throw std::invalid_argument("Payload::get_text: length must be a multiple of 6");
    }
    if (start > bit_count() || length > bit_count() - start) {
        throw std::out_of_range("Payload::get_text: start/length out of range");
    }

    std::string result;
    result.reserve(length / 6);
    for (std::size_t i = 0; i < length; i += 6) {
        const auto value = get_uint(start + i, 6);
        const char c = sixbit_text_alphabet[value];
        if (c == '@') break;
        result.push_back(c);
    }
    const std::size_t last_non_space = result.find_last_not_of(' ');
    if (last_non_space != std::string::npos) {
        result.erase(last_non_space + 1);
    } else {
        result.clear();
    }
    return result;
}

std::uint64_t Payload::get_uint(std::size_t start, std::size_t length) const {
    if (length > 64) {
        throw std::invalid_argument("Payload::get_uint: length exceeds 64 bits");
    }
    if (start > bit_count() || length > bit_count() - start) {
        throw std::out_of_range("Payload::get_uint: start/length out of range");
    }

    std::uint64_t value = 0;
    for (std::size_t i = 0; i < length; ++i) 
        value = (value << 1) | bits_[start + i];
    return value;
}

std::int64_t Payload::get_int(std::size_t start, std::size_t length) const {
    std::uint64_t raw = get_uint(start, length);
    const int shift = 64 - static_cast<int>(length);
    return static_cast<std::int64_t>(raw << shift) >> shift;
}

}  // namespace ais
