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