#pragma once

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

private:
    std::string text_;
    bool valid_;
};

}  // namespace ais
