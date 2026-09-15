#include <gtest/gtest.h>

#include "ais/ais_message.hpp"

TEST(AisMessageTests, Type1Payload) {
    ais::Payload payload(
    "15M67FC000G?ufbE`FepT@3n00Sa", 0);
    auto result = ais::decode_message(payload);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ais::PositionReport>(*result));
    EXPECT_EQ(std::get<ais::PositionReport>(*result).mmsi, 366053209u);
}

TEST(AisMessageTests, Type5Payload) {
    ais::Payload payload(
    "55?MbV02;H;s<HtKR20EHE:0@T4@Dn2222222216L961O5Gf0NSQEp6ClRp8"
    "88888888880", 2);
    auto result = ais::decode_message(payload);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ais::StaticVoyageData>(*result));
    EXPECT_EQ(std::get<ais::StaticVoyageData>(*result).name, "EVER DIADEM");
}

TEST(AisMessageTests, UnsupportedType) {
    ais::Payload payload("4" + std::string(27, '0'), 0);
    auto result = ais::decode_message(payload);
    ASSERT_FALSE(result.has_value());
}

TEST(AisMessageTests, TruncatedMessage) {
    ais::Payload payload("15M67FC000G?ufbEFepT@3n00Sa", 0);
    auto result = ais::decode_message(payload);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(payload.bit_count(), 162u);
}

TEST(AisMessageTests, Type18Payload) {
    ais::Payload payload(
    "B52K>;h00Fc>jpUlNV@ikwpUoP06", 0);
    auto result = ais::decode_message(payload);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ais::PositionReport>(*result));
    EXPECT_EQ(std::get<ais::PositionReport>(*result).mmsi, 338087471);
    EXPECT_EQ(std::get<ais::PositionReport>(*result).message_type, 18);
}

TEST(AisMessageTests, Type24PayloadPartA) {
    ais::Payload payload(
    "H8gQi1@HU<PTpN0pEB08uEV3620", 2);
    auto result = ais::decode_message(payload);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ais::StaticDataPartA>(*result));
    EXPECT_EQ(std::get<ais::StaticDataPartA>(*result).mmsi, 586707205);
    EXPECT_EQ(std::get<ais::StaticDataPartA>(*result).name, "FISHING NET BOUY 1");
}

TEST(AisMessageTests, Type24PayloadPartB) {
    ais::Payload payload(
    "H3n4DU4tC=D6aVJ<;popn0186230", 0);
    auto result = ais::decode_message(payload);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ais::StaticDataPartB>(*result));
    EXPECT_EQ(std::get<ais::StaticDataPartB>(*result).mmsi, 258020500);
    EXPECT_EQ(std::get<ais::StaticDataPartB>(*result).ship_type, 60);
    EXPECT_EQ(std::get<ais::StaticDataPartB>(*result).call_sign, "LK8786");
    EXPECT_EQ(std::get<ais::StaticDataPartB>(*result).to_bow, 9);
    EXPECT_EQ(std::get<ais::StaticDataPartB>(*result).to_stern, 6);
    EXPECT_EQ(std::get<ais::StaticDataPartB>(*result).to_port, 2);
    EXPECT_EQ(std::get<ais::StaticDataPartB>(*result).to_starboard, 3);
}