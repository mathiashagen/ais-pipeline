#include <gtest/gtest.h>

#include "ais/sentence.hpp"
#include "ais/payload.hpp"

TEST(Sentence, ChecksumValidation) {
    // Example of a test that checks the checksum of an AIS sentence.
    const char* validSentence = "!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,0*5C";
    ais::Sentence ais_sentence(validSentence);
    EXPECT_TRUE(ais_sentence.is_valid());

    const char* invalidSentence = "!AIVDM,1,1,,A,15M67FC000G?ufbE`FepT@3n00Sa,0*00";
    ais::Sentence invalid_ais_sentence(invalidSentence);
    EXPECT_FALSE(invalid_ais_sentence.is_valid());
}

TEST(Payload, BitExtraction) {
    const char* payloadStr = "15M67FC000G?ufbE`FepT@3n00Sa";
    ais::Payload payload(payloadStr, 0);
    EXPECT_EQ(payload.bit_count(), 168);
    EXPECT_EQ(payload.get_uint(0, 6), 1);
    EXPECT_EQ(payload.get_uint(8, 30), 366053209);
    EXPECT_THROW(payload.get_uint(160, 30), std::out_of_range);
    EXPECT_THROW(payload.get_uint(0, 100), std::invalid_argument);
}

TEST(Payload, MalformedFillBits) {
    ais::Payload payload("", 3);
    EXPECT_EQ(payload.bit_count(), 0);
}
