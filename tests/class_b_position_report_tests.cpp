#include <gtest/gtest.h>

#include "ais/position_report.hpp"

TEST(ClassBPositionReportTest, DecodeValidReport) {
    const char* payloadStr = "B52K>;h00Fc>jpUlNV@ikwpUoP06";
    ais::Payload payload(payloadStr, 0);
    auto report = ais::decode_class_b_position_report(payload);
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->message_type, 18);
    EXPECT_EQ(report->mmsi, 338087471);
    EXPECT_EQ(report->sog, 0.1);
    EXPECT_FALSE(report->position_accuracy);
    ASSERT_TRUE(report->longitude.has_value());
    EXPECT_NEAR(*report->longitude, -74.07213166, 1e-6);
    ASSERT_TRUE(report->latitude.has_value());
    EXPECT_NEAR(*report->latitude, 40.68454, 1e-6);
    ASSERT_TRUE(report->cog.has_value());
    EXPECT_NEAR(*report->cog, 79.6, 1e-9);
    ASSERT_FALSE(report->true_heading.has_value());
    EXPECT_EQ(report->timestamp, 49);
    EXPECT_TRUE(report->raim);
    EXPECT_EQ(report->rate_of_turn, std::nullopt);
    EXPECT_EQ(report->nav_status, ais::NavStatus::NotDefined);
}

TEST(ClassBPositionReportTest, UnsupportedType) {
    ais::Payload payload("1" + std::string(27, '0'), 0);
    auto result = ais::decode_class_b_position_report(payload);
    ASSERT_FALSE(result.has_value());
}

TEST(ClassBPositionReportTest, TooShort) {
    ais::Payload payload("B" + std::string(10, '0'), 0);
    auto result = ais::decode_class_b_position_report(payload);
    ASSERT_FALSE(result.has_value());
}