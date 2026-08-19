#include <array>
#include <charconv>

#include "ais/sentence.hpp"

namespace ais {

Sentence::Sentence(std::string_view sentence)
    : text_(sentence),   // copy the caller's characters into our own std::string
      valid_(false),      // STUB — replace with the real checksum result below
      well_formed_(false)  // STUB — replace with the real well-formed result below
{
    const std::size_t star = text_.find('*');
    if (star != std::string::npos && star + 2 < text_.size()) {
        unsigned char sum = 0;
        for (std::size_t i = 1; i < star; ++i) {
            sum ^= static_cast<unsigned char>(text_[i]);
        }

        unsigned int expected = 0;
        const char* first = text_.data() + star + 1;
        auto [ptr, ec] = std::from_chars(first, first + 2, expected, 16);

        valid_ = (ec == std::errc{}) && (sum == expected);
    }

    std::array<std::size_t, 6> commas{};
    std::size_t search_from = 0;
    for (int field = 0; field < 6; ++field) {
        search_from = text_.find(',', search_from);
        if (search_from == std::string::npos) {
            well_formed_ = false;
            return;
        }
        commas[field] = search_from;
        ++search_from;
    }
    well_formed_ = true;

    const char* fc_first = text_.data() + commas[0] + 1;
    const char* fc_last = text_.data() + commas[1];
    auto [ptr, ec] = std::from_chars(fc_first, fc_last, fragment_count_);

    const char* fn_first = text_.data() + commas[1] + 1;
    const char* fn_last = text_.data() + commas[2];
    auto [ptr2, ec2] = std::from_chars(fn_first, fn_last, fragment_number_);

    const char* seq_first = text_.data() + commas[2] + 1;
    const char* seq_last = text_.data() + commas[3];
    if (seq_first != seq_last) {
        int seq_id = 0;
        auto [ptr3, ec3] = std::from_chars(seq_first, seq_last, seq_id);
        if (ec3 == std::errc{}) {
            sequence_id_ = seq_id;
        }
    }

    channel_ = std::string_view(text_.data() + commas[3] + 1, commas[4] - (commas[3] + 1));
    payload_ = std::string_view(text_.data() + commas[4] + 1, commas[5] - (commas[4] + 1));

    const char* fill_first = text_.data() + commas[5] + 1;
    const char* fill_last = text_.data() + text_.size();
    auto [ptr4, ec4] = std::from_chars(fill_first, fill_last, fill_bits_);
}
}  // namespace ais
