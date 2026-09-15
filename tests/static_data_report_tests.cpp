#include <gtest/gtest.h>
#include "ais/static_data_report.hpp"

TEST(StaticDataReportTests, DecodesReferenceMessagePartA) {
    ais::Payload payload(
    "H8gQi1@HU<PTpN0pEB08uEV3620", 2);

    auto data = ais::decode_static_data_part_a(payload);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(data->mmsi, 586707205);
    EXPECT_EQ(data->name, "FISHING NET BOUY 1");
}

TEST(StaticDataReportTests, DecodesReferenceMessagePartB) {
    ais::Payload payload(
    "H3n4DU4tC=D6aVJ<;popn0186230", 0);

    auto data = ais::decode_static_data_part_b(payload);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(data->mmsi, 258020500);
    EXPECT_EQ(data->ship_type, 60);
    EXPECT_EQ(data->call_sign, "LK8786");
    EXPECT_EQ(data->to_bow, 9);
    EXPECT_EQ(data->to_stern, 6);
    EXPECT_EQ(data->to_port, 2);
    EXPECT_EQ(data->to_starboard, 3);
}

TEST(StaticDataReportTests, WrongType) {
    ais::Payload payload(
    "15?MbV02;H;s<HtKR20EHE:0@T4@Dn2222222216L961O5Gf0NSQEp6ClRp8"
    "88888888880", 2);
    EXPECT_EQ(payload.bit_count(), 424u);
    EXPECT_FALSE(ais::decode_static_data_part_a(payload));
    EXPECT_FALSE(ais::decode_static_data_part_b(payload));
}

TEST(StaticDataReportTests, DecodeWrongPart) {
    ais::Payload payload("H8gQi1@HU<PTpN0pEB08uEV36200", 0);
    EXPECT_EQ(ais::decode_static_data_part_b(payload), std::nullopt);
    EXPECT_EQ(payload.bit_count(), 168u);
    
    ais::Payload payload_b(
    "H3n4DU4tC=D6aVJ<;popn0186230", 0);
    EXPECT_EQ(ais::decode_static_data_part_a(payload_b), std::nullopt);
}

TEST(StaticDataReportTests, PartARejectsFewerThan160Bits) {
    ais::Payload payload(
    "H8gQi1@HU<PTpN0pEB08uEV3620", 3);
    EXPECT_EQ(payload.bit_count(), 159u);
    auto data = ais::decode_static_data_part_a(payload);
    EXPECT_EQ(data, std::nullopt);
}