#include "ais/sqlite_writer.hpp"
#include "ais/position_reader.hpp"
#include "ais/position_report.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include "ais/concurrent_queue.hpp"
#include <algorithm>
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
    int64_t received_at = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    input.push(report);

    // Stop the writer thread
    input.close();
    writer_thread.join();

    {
        sqlite3* db;
        ASSERT_EQ(sqlite3_open(db_path.c_str(), &db), SQLITE_OK);
        ais::SqliteConnection connection = ais::SqliteConnection(db);

        sqlite3_stmt* stmt;
        ASSERT_EQ(sqlite3_prepare_v2(connection.get(), "SELECT mmsi, latitude, longitude, sog, cog, true_heading, timestamp, message_type, nav_status, received_at FROM position_reports", -1, &stmt, nullptr), SQLITE_OK);

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
        EXPECT_NEAR(sqlite3_column_int64(statement.get(), 9), received_at, 10);

        ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_DONE);
    }
}

// More reports than the queue holds or one batch takes, pushed as fast as the
// queue allows: every report must land, and within the burst the order must
// survive batching -- each ship's latest position is the last one pushed.
TEST(SqliteWriterTest, WritesABurstLargerThanOneBatchInOrder) {
    std::string db_path = (std::filesystem::temp_directory_path() / "test.db").string();
    std::filesystem::remove(db_path);
    ais::SqliteWriter writer(db_path);

    constexpr int ships = 300;
    constexpr int reports = 1200;

    ais::ThreadSafeQueue<ais::PositionReport> input(500);
    std::jthread writer_thread([&writer, &input](std::stop_token stop_token) {
        writer.run(input, stop_token);
    });

    for (int i = 0; i < reports; ++i) {
        ais::PositionReport report;
        report.mmsi = 100000000 + i % ships;
        report.latitude = static_cast<double>(i);  // encodes the push order
        report.longitude = 10.0;
        input.push(report);
    }
    input.close();
    writer_thread.join();

    ais::PositionReader reader(db_path);
    auto latest = reader.latest_positions();
    ASSERT_EQ(latest.size(), static_cast<std::size_t>(ships));

    for (const auto& record : latest) {
        const int ship = static_cast<int>(record.report.mmsi - 100000000);
        // The last report pushed for ship k was number (reports - ships + k).
        EXPECT_EQ(record.report.latitude, static_cast<double>(reports - ships + ship))
            << "ship " << record.report.mmsi;
    }

    sqlite3* db;
    ASSERT_EQ(sqlite3_open(db_path.c_str(), &db), SQLITE_OK);
    ais::SqliteConnection connection(db);
    sqlite3_stmt* raw;
    ASSERT_EQ(sqlite3_prepare_v2(connection.get(), "SELECT count(*) FROM position_reports", -1, &raw, nullptr), SQLITE_OK);
    ais::SqliteStatement count(raw);
    ASSERT_EQ(sqlite3_step(count.get()), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(count.get(), 0), reports);
}

// A database written before latest_positions existed has history but no
// latest table. Opening it with the writer must fill the table from that
// history, once.
TEST(SqliteWriterTest, BackfillsLatestPositionsFromExistingHistory) {
    std::string db_path = (std::filesystem::temp_directory_path() / "test.db").string();
    std::filesystem::remove(db_path);

    {
        sqlite3* db;
        ASSERT_EQ(sqlite3_open(db_path.c_str(), &db), SQLITE_OK);
        ais::SqliteConnection connection(db);
        char* err_msg = nullptr;
        const char* old_database = R"sql(
            CREATE TABLE position_reports (mmsi INTEGER, latitude REAL, longitude REAL, sog REAL, cog REAL,
                true_heading REAL, timestamp INTEGER, message_type INTEGER, nav_status INTEGER, received_at INTEGER);

            -- Ship 1: two positions; the newer one is its latest.
            INSERT INTO position_reports (mmsi, latitude, longitude, received_at) VALUES (111111111, 10.0, 20.0, 1000);
            INSERT INTO position_reports (mmsi, latitude, longitude, received_at) VALUES (111111111, 11.0, 21.0, 2000);

            -- Ship 2: its newest report has no position, so its latest is the
            -- one before. Ranking all rows first and filtering afterwards would
            -- lose this ship entirely.
            INSERT INTO position_reports (mmsi, latitude, longitude, received_at) VALUES (222222222, 12.0, 22.0, 1000);
            INSERT INTO position_reports (mmsi, latitude, longitude, received_at) VALUES (222222222, NULL, NULL, 3000);

            -- Ship 3: never reported a position, so it has no latest position.
            INSERT INTO position_reports (mmsi, latitude, longitude, received_at) VALUES (333333333, NULL, NULL, 1000);
        )sql";
        ASSERT_EQ(sqlite3_exec(connection.get(), old_database, nullptr, nullptr, &err_msg), SQLITE_OK) << err_msg;
    }

    // Twice: the second open must not backfill again and duplicate or reset rows.
    { ais::SqliteWriter writer(db_path); }
    ais::SqliteWriter writer(db_path);
    ais::PositionReader reader(db_path);

    auto latest = reader.latest_positions();
    ASSERT_EQ(latest.size(), 2);

    auto find = [&latest](std::uint32_t mmsi) {
        return std::find_if(latest.begin(), latest.end(),
            [mmsi](const ais::PositionRecord& r) { return r.report.mmsi == mmsi; });
    };

    auto ship1 = find(111111111);
    ASSERT_NE(ship1, latest.end());
    EXPECT_EQ(ship1->report.latitude, 11.0);
    EXPECT_EQ(ship1->received_at, 2000);

    auto ship2 = find(222222222);
    ASSERT_NE(ship2, latest.end());
    EXPECT_EQ(ship2->report.latitude, 12.0);
    EXPECT_EQ(ship2->received_at, 1000);

    EXPECT_EQ(find(333333333), latest.end());
}