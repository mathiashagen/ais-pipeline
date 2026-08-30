#include "ais/sqlite_writer.hpp"
#include "ais/position_report.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include "ais/concurrent_queue.hpp"
#include <thread>

TEST(SqliteWriterTest, BasicTest) {
    // Create a temporary SQLite database in memory
    std::string db_path = (std::filesystem::temp_directory_path() / "test.db").string();
    std::filesystem::remove(db_path);
    ais::SqliteWriter writer(db_path);

    ais::PositionReport report;
    report.mmsi = 123456789;
    report.latitude = 37.7749;
    report.longitude = -122.4194;
    report.sog = 12.5;
    report.cog = 85.0;
    report.true_heading = 90;
    report.timestamp = 18;
    report.message_type = 1;
    report.nav_status = ais::NavStatus::UnderWayEngine;

    ais::ThreadSafeQueue<ais::PositionReport> input(500);
    std::jthread writer_thread([&writer, &input](std::stop_token stop_token) {
        writer.run(input, stop_token);
    });

    // Push the report to the input queue
    input.push(report);

    // Stop the writer thread
    input.close();
    writer_thread.join();

    {
        sqlite3* db;
        ASSERT_EQ(sqlite3_open(db_path.c_str(), &db), SQLITE_OK);
        ais::SqliteConnection connection = ais::SqliteConnection(db);

        sqlite3_stmt* stmt;
        ASSERT_EQ(sqlite3_prepare_v2(connection.get(), "SELECT mmsi, latitude, longitude, sog, cog, true_heading, timestamp, message_type, nav_status FROM position_reports", -1, &stmt, nullptr), SQLITE_OK);

        ais::SqliteStatement statement = ais::SqliteStatement(stmt);

        ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_ROW);
        EXPECT_EQ(sqlite3_column_int64(statement.get(), 0), report.mmsi);
        EXPECT_EQ(sqlite3_column_double(statement.get(), 1), report.latitude);
        EXPECT_EQ(sqlite3_column_double(statement.get(), 2), report.longitude);
        EXPECT_EQ(sqlite3_column_double(statement.get(), 3), report.sog);
        EXPECT_EQ(sqlite3_column_double(statement.get(), 4), report.cog);
        EXPECT_EQ(sqlite3_column_int(statement.get(), 5), report.true_heading);
        EXPECT_EQ(sqlite3_column_int(statement.get(), 6), report.timestamp);
        EXPECT_EQ(sqlite3_column_int(statement.get(), 7), report.message_type);
        EXPECT_EQ(sqlite3_column_int(statement.get(), 8), static_cast<int>(report.nav_status));

        ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_DONE);
    }
}