#include <gtest/gtest.h>

#include "ais/sentence.hpp"
#include "ais/payload.hpp"
#include "ais/position_report.hpp"

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

TEST(Payload, SignedInt) {
    const char* payloadStr = "15M67FC000G?ufbE`FepT@3n00Sa";
    ais::Payload payload(payloadStr, 0);
    EXPECT_EQ(payload.get_int(61, 28), -73404971);   // longitude field
    EXPECT_EQ(payload.get_int(89, 27), 22681271);    // latitude field, positive
}

TEST(PositionReport, DecodesValidMessage) {
    const char* payloadStr = "15M67FC000G?ufbE`FepT@3n00Sa";
    ais::Payload payload(payloadStr, 0);
    auto result = ais::decode_position_report(payload);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, 1);
    EXPECT_EQ(result->mmsi, 366053209u);
    EXPECT_EQ(result->nav_status, ais::NavStatus::RestrictedManeuver);
    ASSERT_TRUE(result->sog.has_value());
    EXPECT_DOUBLE_EQ(*result->sog, 0.0);
    EXPECT_FALSE(result->position_accuracy);
    ASSERT_TRUE(result->longitude.has_value());
    EXPECT_NEAR(*result->longitude, -122.341618, 1e-6);
    ASSERT_TRUE(result->latitude.has_value());
    EXPECT_NEAR(*result->latitude, 37.802118, 1e-6);
    ASSERT_TRUE(result->cog.has_value());
    EXPECT_NEAR(*result->cog, 219.3, 1e-9);
    ASSERT_TRUE(result->true_heading.has_value());
    EXPECT_EQ(*result->true_heading, 1);
    ASSERT_TRUE(result->rate_of_turn.has_value());
    EXPECT_EQ(*result->rate_of_turn, 0);
    EXPECT_EQ(result->timestamp, 59);
    EXPECT_EQ(result->maneuver_indicator, 0);
    EXPECT_FALSE(result->raim);
    EXPECT_EQ(result->radio_status, 2281u);
}