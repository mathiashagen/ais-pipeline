#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ais {

// Represents a single AIVDM/AIVDO sentence. For now it only knows whether its
// trailing "*HH" checksum is valid; in later Fase 1 slices it will grow to
// expose the payload, message type, MMSI, position, etc.
//
// Ownership: the Sentence OWNS its text (a std::string copy). It does not
// borrow the caller's characters, so its lifetime is self-contained and there
// is nothing to dangle. Copying one short line is cheap.
class Sentence {
public:
    // Validation is computed eagerly here in the constructor and cached in
    // valid_, so an invalid sentence is knowable the moment you have one.
    explicit Sentence(std::string_view sentence);

    // const: inspecting validity never mutates the Sentence.
    bool is_valid() const { return valid_; }
    bool is_well_formed() const { return well_formed_; }

    int fragment_count() const { return fragment_count_; }
    int fragment_number() const { return fragment_number_; }
    std::optional<int> sequence_id() const { return sequence_id_; }
    std::string_view channel() const { return channel_; }
    std::string_view payload() const { return payload_; }
    int fill_bits() const { return fill_bits_; }

private:
    std::string text_;
    bool valid_;
    bool well_formed_ = false;
    int fragment_count_ = 0;
    int fragment_number_ = 0;
    std::optional<int> sequence_id_;
    std::string_view channel_;
    std::string_view payload_;
    int fill_bits_ = 0;
};

}  // namespace ais
