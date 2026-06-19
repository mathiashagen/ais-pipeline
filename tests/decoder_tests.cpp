#include <gtest/gtest.h>

#include "ais/sentence.hpp"

TEST(Sentence, ChecksumValidation) {
    // Example of a test that checks the checksum of an AIS sentence.
    const char* validSentence = "!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,0*5C";
    ais::Sentence ais_sentence(validSentence);
    EXPECT_TRUE(ais_sentence.is_valid());

    const char* invalidSentence = "!AIVDM,1,1,,A,15M67FC000G?ufbE`FepT@3n00Sa,0*00";
    ais::Sentence invalid_ais_sentence(invalidSentence);
    EXPECT_FALSE(invalid_ais_sentence.is_valid());
}
