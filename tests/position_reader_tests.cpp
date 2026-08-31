#include "gtest/gtest.h"
#include "ais/position_reader.hpp"
#include "ais/sqlite_writer.hpp"
#include "ais/position_report.hpp"
#include <filesystem>
#include <algorithm>

TEST(PositionReaderTests, History) {
    std::string db_path = (std::filesystem::temp_directory_path() / "test.db").string();
    std::filesystem::remove(db_path);
    ais::SqliteWriter writer(db_path);
    ais::PositionReader reader(db_path);

    sqlite3* seed_db;
    sqlite3_open(db_path.c_str(), &seed_db);
    char* err_msg = nullptr;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (123456789, 20.0, 10.0, 100.0, 90, 1000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (123456789, 21.0, 11.0, 110.0, 100, 2000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (987654321, 22.0, 12.0, 120.0, 110, 3000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (123456789, 23.0, 11.0, 110.0, 100, 4000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    sqlite3_close(seed_db);

    auto history = reader.history(123456789, 2);
    ASSERT_EQ(history.size(), 2);
    EXPECT_EQ(history[0].report.mmsi, 123456789);
    EXPECT_EQ(history[1].report.mmsi, 123456789);
    EXPECT_EQ(history[0].report.longitude, 23.0);
    EXPECT_EQ(history[0].report.latitude, 11.0);
    EXPECT_EQ(history[1].report.longitude, 21.0);
    EXPECT_EQ(history[1].report.latitude, 11.0);
    EXPECT_EQ(history[0].report.cog, 110.0);
    EXPECT_EQ(history[0].report.true_heading, 100);
    EXPECT_EQ(history[1].report.cog, 110.0);
    EXPECT_EQ(history[1].report.true_heading, 100);
    EXPECT_EQ(history[0].received_at, 4000);
    EXPECT_EQ(history[1].received_at, 2000);
}

auto find_by_mmsi = [](const std::vector<ais::PositionRecord>& records, std::uint32_t mmsi) {
    auto it = std::find_if(records.begin(), records.end(),
        [mmsi](const ais::PositionRecord& r) { return r.report.mmsi == mmsi; });
    return it;
};

TEST(PositionReaderTests, LatestPositions) {
    std::string db_path = (std::filesystem::temp_directory_path() / "test.db").string();
    std::filesystem::remove(db_path);
    ais::SqliteWriter writer(db_path);
    ais::PositionReader reader(db_path);

    sqlite3* seed_db;
    sqlite3_open(db_path.c_str(), &seed_db);
    char* err_msg = nullptr;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (123456789, 20.0, 10.0, 100.0, 90, 1000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (123456789, 21.0, 11.0, 110.0, 100, 4000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (987654321, 22.0, 12.0, 120.0, 110, 3000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (123456789, 23.0, 11.0, 110.0, 100, 4000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    sqlite3_close(seed_db);

    auto latest_positions = reader.latest_positions();
    ASSERT_EQ(latest_positions.size(), 2);
    auto it_123456789 = find_by_mmsi(latest_positions, 123456789);
    ASSERT_NE(it_123456789, latest_positions.end());
    EXPECT_EQ(it_123456789->report.mmsi, 123456789);
    EXPECT_EQ(it_123456789->report.longitude, 23.0);
    EXPECT_EQ(it_123456789->report.latitude, 11.0);
    EXPECT_EQ(it_123456789->report.cog, 110.0);
    EXPECT_EQ(it_123456789->report.true_heading, 100);
    EXPECT_EQ(it_123456789->received_at, 4000);

    auto it_987654321 = find_by_mmsi(latest_positions, 987654321);
    ASSERT_NE(it_987654321, latest_positions.end());
    EXPECT_EQ(it_987654321->report.mmsi, 987654321);
    EXPECT_EQ(it_987654321->report.longitude, 22.0);
    EXPECT_EQ(it_987654321->report.latitude, 12.0);
    EXPECT_EQ(it_987654321->report.cog, 120.0);
    EXPECT_EQ(it_987654321->report.true_heading, 110);
    EXPECT_EQ(it_987654321->received_at, 3000);
}

TEST(PositionReaderTests, PositionsInArea) {
    std::string db_path = (std::filesystem::temp_directory_path() / "test.db").string();
    std::filesystem::remove(db_path);
    ais::SqliteWriter writer(db_path);
    ais::PositionReader reader(db_path);
    sqlite3* seed_db;
    sqlite3_open(db_path.c_str(), &seed_db);
    char* err_msg = nullptr;

    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (123456789, 21.0, 11.0, 100.0, 90, 1000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (123456789, 25.0, 15.0, 110.0, 100, 4000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (987654321, 26.0, 16.0, 120.0, 110, 5000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    ASSERT_EQ(sqlite3_exec(seed_db, "INSERT INTO position_reports (mmsi, longitude, latitude, cog, true_heading, received_at) VALUES (987654321, 20.5, 11.0, 110.0, 100, 7000);", nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    sqlite3_close(seed_db);

    ais::BoundingBox box{10.0, 12.0, 20.0, 22.0};
    auto positions_in_area = reader.positions_in_area(box);
    ASSERT_EQ(positions_in_area.size(), 1);
    auto it_123456789 = find_by_mmsi(positions_in_area, 123456789);
    ASSERT_EQ(it_123456789, positions_in_area.end());

    auto it_987654321 = find_by_mmsi(positions_in_area, 987654321);
    ASSERT_NE(it_987654321, positions_in_area.end());
    EXPECT_EQ(it_987654321->report.mmsi, 987654321);
    EXPECT_EQ(it_987654321->report.longitude, 20.5);
    EXPECT_EQ(it_987654321->report.latitude, 11.0);
}