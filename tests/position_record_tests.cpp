#include <gtest/gtest.h>
#include "ais/position_record.hpp"

TEST(PositionRecordTest, ToJson) {
    ais::PositionRecord record;
    record.report.mmsi = 123456789;
    record.report.latitude = 12.34;
    record.report.longitude = 56.78;
    record.report.timestamp = 16;
    record.report.nav_status = ais::NavStatus::UnderWayEngine;
    record.report.true_heading = 90;
    record.received_at = 1617181930;

    nlohmann::json j = record;
    EXPECT_EQ(j["mmsi"], 123456789);
    EXPECT_EQ(j["latitude"], 12.34);
    EXPECT_EQ(j["longitude"], 56.78);
    EXPECT_EQ(j["timestamp"], 16);
    EXPECT_EQ(j["nav_status"], static_cast<int>(ais::NavStatus::UnderWayEngine));
    EXPECT_TRUE(j["sog"].is_null());
    EXPECT_TRUE(j["cog"].is_null());
    EXPECT_EQ(j["true_heading"], 90);
    EXPECT_EQ(j["received_at"], 1617181930);
}