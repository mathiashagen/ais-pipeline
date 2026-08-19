#include <gtest/gtest.h>

#include "ais/sentence.hpp"
#include "ais/payload.hpp"
#include "ais/position_report.hpp"
#include "ais/decoder.hpp"
#include "ais/sentence_assembler.hpp"

TEST(Sentence, ChecksumValidation) {
    // Example of a test that checks the checksum of an AIS sentence.
    const char* validSentence = "!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,0*5C";
    ais::Sentence ais_sentence(validSentence);
    EXPECT_TRUE(ais_sentence.is_valid());

    const char* invalidSentence = "!AIVDM,1,1,,A,15M67FC000G?ufbE`FepT@3n00Sa,0*00";
    ais::Sentence invalid_ais_sentence(invalidSentence);
    EXPECT_FALSE(invalid_ais_sentence.is_valid());
}

TEST(Sentence, FieldExtraction) {
    const char* s = "!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,0*5C";
    ais::Sentence sentence(s);
    EXPECT_TRUE(sentence.is_well_formed());
    EXPECT_EQ(sentence.fragment_count(), 1);
    EXPECT_EQ(sentence.fragment_number(), 1);
    EXPECT_FALSE(sentence.sequence_id().has_value());
    EXPECT_EQ(sentence.channel(), "B");
    EXPECT_EQ(sentence.payload(), "15M67FC000G?ufbE`FepT@3n00Sa");
    EXPECT_EQ(sentence.fill_bits(), 0);
}

TEST(Sentence, WellFormed) {
    ais::Sentence sentence("!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,0*5C");
    EXPECT_TRUE(sentence.is_well_formed());

    ais::Sentence too_few("not,enough,commas*00");   // has a checksum suffix now
    EXPECT_FALSE(too_few.is_well_formed());

    ais::Sentence no_checksum("not,enough,commas");   // no '*' at all
    EXPECT_FALSE(no_checksum.is_well_formed());
}

TEST(Sentence, FillBitsNonZero) {
    ais::Sentence s("!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,2*5C");
    EXPECT_EQ(s.fill_bits(), 2);
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

TEST(Decoder, DecodesFullSentence) {
    const char* s = "!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,0*5C";
    ais::Sentence sentence(s);
    auto result = ais::decode_sentence(sentence);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mmsi, 366053209u);
}

TEST(Decoder, RejectsBadChecksum) {
    const char* s = "!AIVDM,1,1,,A,15M67FC000G?ufbE`FepT@3n00Sa,0*00";
    ais::Sentence sentence(s);
    auto result = ais::decode_sentence(sentence);
    EXPECT_FALSE(result.has_value());
}

TEST(SentenceAssembler, ReassemblesTwoParts) {
    ais::SentenceAssembler assembler;

    ais::Sentence part1("!AIVDM,2,1,5,B,15M67FC000G?uf,0*05");
    auto result1 = assembler.add(part1);
    EXPECT_FALSE(result1.has_value());   // still waiting on part 2

    ais::Sentence part2("!AIVDM,2,2,5,B,bE`FepT@3n00Sa,0*7F");
    auto result2 = assembler.add(part2);
    ASSERT_TRUE(result2.has_value());

    EXPECT_EQ(result2->bit_count(), 168);
    auto report = ais::decode_position_report(*result2);
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->mmsi, 366053209u);
}

TEST(SentenceAssembler, PassesThroughSinglePart) {
    ais::SentenceAssembler assembler;
    ais::Sentence sentence("!AIVDM,1,1,,B,15M67FC000G?ufbE`FepT@3n00Sa,0*5C");
    auto result = assembler.add(sentence);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bit_count(), 168);
}