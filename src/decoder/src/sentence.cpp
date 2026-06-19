#include <charconv>

#include "ais/sentence.hpp"

namespace ais {

Sentence::Sentence(std::string_view sentence)
    : text_(sentence),   // copy the caller's characters into our own std::string
      valid_(false)      // STUB — replace with the real checksum result below
{
    // Your job (Fase 1, slice 1): compute the checksum and assign it to valid_.
    //
    //   1. Find '!' and '*' in text_ (std::string has .find / .substr too).
    //   2. XOR every byte strictly between them into an unsigned accumulator.
    //   3. Parse the two hex digits after '*' (std::from_chars, base 16).
    //   4. valid_ = (accumulator == parsed hex), guarding the malformed cases.
    //
    // Until you do, valid_ stays false so your "valid sentence" test fails (red).

    const std::size_t star = text_.find('*');
    if (star == std::string::npos || star + 2 >= text_.size()) {
        return; // no '*', or no room for two hex digits -> valid_ stays false
    }

    unsigned char sum = 0;
    for (std::size_t i = 1; i < star; ++i) {
        sum ^= static_cast<unsigned char>(text_[i]);
    }

    unsigned int expected = 0;
    const char* first = text_.data() + star + 1;
    auto [ptr, ec] = std::from_chars(first, first + 2, expected, 16);

    valid_ = (ec == std::errc{}) && (sum == expected);
}

}  // namespace ais
