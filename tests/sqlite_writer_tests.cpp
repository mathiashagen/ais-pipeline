#include "ais/sqlite_writer.hpp"
#include "ais/position_reader.hpp"
#include "ais/position_report.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include "ais/concurrent_queue.hpp"
#include <algorithm>
#include <optional>
#include <string>
#include <thread>
#include <vector>

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

    ais::ThreadSafeQueue<ais::AisMessage> input(500);
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

    ais::ThreadSafeQueue<ais::AisMessage> input(500);
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

TEST(SqliteWriterTest, MixedBatchTest) {
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

    ais::StaticVoyageData static_data;
    static_data.mmsi = 123456789;
    static_data.name = "EVER DIADEM";

    ais::PositionReport report2;
    report2.mmsi = 987654321;
    report2.latitude = 40.7128;
    report2.longitude = -74.0060;
    report2.sog = 10.0;
    report2.cog = 90.0;
    report2.true_heading = 85;
    report2.timestamp = 20;
    report2.message_type = 1;
    report2.nav_status = ais::NavStatus::UnderWayEngine;

    ais::ThreadSafeQueue<ais::AisMessage> input(500);
    input.push(report);
    input.push(static_data);
    input.push(report2);
    std::jthread writer_thread([&writer, &input](std::stop_token stop_token) {
        writer.run(input, stop_token);
    });

    // Stop the writer thread
    input.close();
    writer_thread.join();

    {
        sqlite3* db;
        ASSERT_EQ(sqlite3_open(db_path.c_str(), &db), SQLITE_OK);
        ais::SqliteConnection connection = ais::SqliteConnection(db);

        sqlite3_stmt* stmt;
        ASSERT_EQ(sqlite3_prepare_v2(connection.get(), "SELECT count(*) FROM position_reports", -1, &stmt, nullptr), SQLITE_OK);

        ais::SqliteStatement statement = ais::SqliteStatement(stmt);
        ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_ROW);
        EXPECT_EQ(sqlite3_column_int(statement.get(), 0), 2);

        ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_DONE);

        ASSERT_EQ(sqlite3_prepare_v2(connection.get(), "SELECT count(*) FROM ship_static", -1, &stmt, nullptr), SQLITE_OK);

        statement = ais::SqliteStatement(stmt);
        ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_ROW);
        EXPECT_EQ(sqlite3_column_int(statement.get(), 0), 1);

        ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_DONE);
    }
}

TEST(SqliteWriterTest, BasicShipStaticVoyageDataTest) {
    std::string db_path = (std::filesystem::temp_directory_path() / "test.db").string();
    std::filesystem::remove(db_path);
    ais::SqliteWriter writer(db_path);

    ais::StaticVoyageData static_data;
    static_data.mmsi = 123456789;
    static_data.name = "EVER DIADEM";
    static_data.draught = 12.2;
    static_data.destination = "OSLO";

    ais::StaticVoyageData static_data2;
    static_data2.mmsi = 123456789;
    static_data2.name = "EVER DIADEM";
    static_data2.draught = 12.2;
    static_data2.destination = "TRONDHEIM";

    ais::ThreadSafeQueue<ais::AisMessage> input(500);
    input.push(static_data);
    input.push(static_data2);
    std::jthread writer_thread([&writer, &input](std::stop_token stop_token) {
        writer.run(input, stop_token);
    });

    // Stop the writer thread
    input.close();
    writer_thread.join();

    {
        sqlite3* db;
        ASSERT_EQ(sqlite3_open(db_path.c_str(), &db), SQLITE_OK);
        ais::SqliteConnection connection = ais::SqliteConnection(db);

        sqlite3_stmt* stmt;
        ASSERT_EQ(sqlite3_prepare_v2(connection.get(), "SELECT mmsi, name, draught, imo, destination, call_sign, eta_hour FROM ship_static", -1, &stmt, nullptr), SQLITE_OK);

        ais::SqliteStatement statement = ais::SqliteStatement(stmt);
        ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_ROW);
        EXPECT_EQ(sqlite3_column_int(statement.get(), 0), 123456789);
        EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1)), "EVER DIADEM");
        EXPECT_DOUBLE_EQ(sqlite3_column_double(statement.get(), 2), 12.2);  // draught set to 12.2
        EXPECT_EQ(sqlite3_column_type(statement.get(), 3), SQLITE_NULL);  // imo default
        EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 4)), "TRONDHEIM");  // destination set to "TRONDHEIM"
        EXPECT_EQ(sqlite3_column_type(statement.get(), 5), SQLITE_NULL);  // call_sign default
        EXPECT_EQ(sqlite3_column_type(statement.get(), 6), SQLITE_NULL);  // eta_hour default

        ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_DONE);
    }
}

namespace {

// Runs a writer over messages queued before it starts, so they all land in
// one batch and one transaction, in the order given.
void write_messages(const std::string& db_path, const std::vector<ais::AisMessage>& messages) {
    ais::SqliteWriter writer(db_path);
    ais::ThreadSafeQueue<ais::AisMessage> input(500);
    for (const ais::AisMessage& message : messages) {
        input.push(message);
    }
    std::jthread writer_thread([&writer, &input](std::stop_token stop_token) {
        writer.run(input, stop_token);
    });
    input.close();
    writer_thread.join();
}

struct ShipStaticRow {
    std::optional<std::string> name;
    std::optional<std::string> call_sign;
    std::optional<int> ship_type;
    std::optional<int> to_bow;
    std::optional<int> to_stern;
    std::optional<int> to_port;
    std::optional<int> to_starboard;
};

std::optional<std::string> text_column(sqlite3_stmt* stmt, int index) {
    if (sqlite3_column_type(stmt, index) == SQLITE_NULL) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, index)));
}

std::optional<int> int_column(sqlite3_stmt* stmt, int index) {
    if (sqlite3_column_type(stmt, index) == SQLITE_NULL) return std::nullopt;
    return sqlite3_column_int(stmt, index);
}

// Every ship_static row, so a test can check both what one ship holds and
// that an upsert did not add a second row for it.
std::vector<ShipStaticRow> read_ship_static(const std::string& db_path) {
    sqlite3* db = nullptr;
    sqlite3_open(db_path.c_str(), &db);
    ais::SqliteConnection connection(db);

    sqlite3_stmt* raw = nullptr;
    sqlite3_prepare_v2(connection.get(),
        "SELECT name, call_sign, ship_type, to_bow, to_stern, to_port, to_starboard FROM ship_static ORDER BY mmsi",
        -1, &raw, nullptr);
    ais::SqliteStatement statement(raw);

    std::vector<ShipStaticRow> rows;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        rows.push_back({
            text_column(statement.get(), 0),
            text_column(statement.get(), 1),
            int_column(statement.get(), 2),
            int_column(statement.get(), 3),
            int_column(statement.get(), 4),
            int_column(statement.get(), 5),
            int_column(statement.get(), 6),
        });
    }
    return rows;
}

std::string fresh_db_path() {
    std::string db_path = (std::filesystem::temp_directory_path() / "test.db").string();
    std::filesystem::remove(db_path);
    return db_path;
}

ais::StaticDataPartA part_a(std::uint32_t mmsi, std::string name) {
    ais::StaticDataPartA data;
    data.mmsi = mmsi;
    data.name = std::move(name);
    return data;
}

ais::StaticDataPartB part_b(std::uint32_t mmsi) {
    ais::StaticDataPartB data;
    data.mmsi = mmsi;
    data.ship_type = 60;
    data.call_sign = "LK8786";
    data.to_bow = 9;
    data.to_stern = 6;
    data.to_port = 2;
    data.to_starboard = 3;
    return data;
}

void expect_part_b_columns(const ShipStaticRow& row) {
    EXPECT_EQ(row.ship_type, 60);
    EXPECT_EQ(row.call_sign, "LK8786");
    EXPECT_EQ(row.to_bow, 9);
    EXPECT_EQ(row.to_stern, 6);
    EXPECT_EQ(row.to_port, 2);
    EXPECT_EQ(row.to_starboard, 3);
}

}  // namespace

// Part B carries no name, so its upsert must leave the name part A stored.
TEST(SqliteWriterTest, PartBKeepsNameFromPartA) {
    const std::string db_path = fresh_db_path();
    write_messages(db_path, {part_a(258020500, "FISHING NET BOUY 1"), part_b(258020500)});

    const auto rows = read_ship_static(db_path);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].name, "FISHING NET BOUY 1");
    expect_part_b_columns(rows[0]);
}

// The reverse order: part A must leave the type, call sign and dimensions.
// Checking part B's columns is the point -- they are the ones stored first,
// so the ones a whole-row replace would wipe.
TEST(SqliteWriterTest, PartAKeepsTypeAndDimensionsFromPartB) {
    const std::string db_path = fresh_db_path();
    write_messages(db_path, {part_b(258020500), part_a(258020500, "FISHING NET BOUY 1")});

    const auto rows = read_ship_static(db_path);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].name, "FISHING NET BOUY 1");
    expect_part_b_columns(rows[0]);
}

TEST(SqliteWriterTest, PartBAloneLeavesNameNull) {
    const std::string db_path = fresh_db_path();
    write_messages(db_path, {part_b(258020500)});

    const auto rows = read_ship_static(db_path);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].name, std::nullopt);
    expect_part_b_columns(rows[0]);
}

TEST(SqliteWriterTest, LaterPartAReplacesName) {
    const std::string db_path = fresh_db_path();
    write_messages(db_path, {part_a(258020500, "OLD NAME"), part_a(258020500, "NEW NAME")});

    const auto rows = read_ship_static(db_path);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].name, "NEW NAME");
    EXPECT_EQ(rows[0].ship_type, std::nullopt);
}
