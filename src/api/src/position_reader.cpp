#include "ais/position_reader.hpp"
#include "ais/sqlite_writer.hpp"

#include <stdexcept>

namespace ais {

namespace {

// Resets a statement on scope exit, including when an exception unwinds past
// it. A stepped statement that is never reset keeps its read transaction open,
// which in WAL mode pins a snapshot and stops the writer checkpointing.
class StatementReset {
public:
    explicit StatementReset(sqlite3_stmt* stmt) : stmt_(stmt) {}
    ~StatementReset() { sqlite3_reset(stmt_); }

    StatementReset(const StatementReset&) = delete;
    StatementReset& operator=(const StatementReset&) = delete;

private:
    sqlite3_stmt* stmt_;
};

}  // namespace

PositionReader::PositionReader(std::string db_path) {
    sqlite3* db = nullptr;
    const int rc = sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    // sqlite3_open_v2 hands back a handle that must be closed even when it
    // fails, so take ownership before checking the result.
    connection_ = SqliteConnection(db);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }

    // Per-connection, so the writer's timeout does not cover this one.
    sqlite3_busy_timeout(connection_.get(), 5000);

    sqlite3_stmt* history_stmt = nullptr;
    const char* history_sql =
    "SELECT mmsi, latitude, longitude, sog, cog, true_heading, timestamp, message_type, nav_status, received_at FROM position_reports WHERE mmsi = ? ORDER BY received_at DESC LIMIT ?";
    if (sqlite3_prepare_v2(db, history_sql, -1, &history_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    history_statement_ = SqliteStatement(history_stmt);

    // Both read latest_positions, which SqliteWriter keeps at one row per ship
    // with a trigger. It stays the size of the fleet however long the pipeline
    // runs, where finding each ship's newest row in position_reports meant
    // ranking the entire history on every request. Column order must match
    // read_current_row, which reads by position.
    sqlite3_stmt* latest_positions_stmt = nullptr;
    const char* latest_positions_sql =
    "SELECT mmsi, latitude, longitude, sog, cog, true_heading, timestamp, message_type, nav_status, received_at "
    "FROM latest_positions";
    if (sqlite3_prepare_v2(db, latest_positions_sql, -1, &latest_positions_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    latest_positions_statement_ = SqliteStatement(latest_positions_stmt);

    sqlite3_stmt* positions_in_area_stmt = nullptr;
    const char* positions_in_area_sql =
    "SELECT mmsi, latitude, longitude, sog, cog, true_heading, timestamp, message_type, nav_status, received_at "
    "FROM latest_positions "
    "WHERE latitude BETWEEN ? AND ? AND longitude BETWEEN ? AND ?";
    if (sqlite3_prepare_v2(db, positions_in_area_sql, -1, &positions_in_area_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    positions_in_area_statement_ = SqliteStatement(positions_in_area_stmt);
}

// Steps a bound statement to exhaustion. Distinguishes "no more rows" from a
// failure: sqlite3_step returns SQLITE_BUSY, SQLITE_ERROR and SQLITE_CORRUPT
// through the same path as SQLITE_DONE, so a loop that only tests for
// SQLITE_ROW reports a database failure as an empty result set.
std::vector<PositionRecord> PositionReader::collect_rows(sqlite3_stmt* stmt) {
    StatementReset reset_on_exit(stmt);

    std::vector<PositionRecord> results;

    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        results.push_back(read_current_row(stmt));
    }
    if (rc != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(connection_.get()));
    }

    return results;
}

std::vector<PositionRecord> PositionReader::history(std::uint32_t mmsi, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = history_statement_.get();
    sqlite3_bind_int(stmt, 1, mmsi);
    sqlite3_bind_int(stmt, 2, limit);

    return collect_rows(stmt);
}

std::vector<PositionRecord> PositionReader::latest_positions() {
    std::lock_guard<std::mutex> lock(mutex_);

    return collect_rows(latest_positions_statement_.get());
}

std::vector<PositionRecord> PositionReader::positions_in_area(BoundingBox box) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* stmt = positions_in_area_statement_.get();
    sqlite3_bind_double(stmt, 1, box.min_lat);
    sqlite3_bind_double(stmt, 2, box.max_lat);
    sqlite3_bind_double(stmt, 3, box.min_lon);
    sqlite3_bind_double(stmt, 4, box.max_lon);

    return collect_rows(stmt);
}

PositionRecord PositionReader::read_current_row(sqlite3_stmt* stmt) {
    PositionRecord record;
    record.report.mmsi = sqlite3_column_int(stmt, 0);
    if (sqlite3_column_type(stmt, 1) == SQLITE_NULL) {
        record.report.latitude = std::nullopt;
    } else {
        record.report.latitude = sqlite3_column_double(stmt, 1); 
    }
    if (sqlite3_column_type(stmt, 2) == SQLITE_NULL) {
        record.report.longitude = std::nullopt;
    } else {
        record.report.longitude = sqlite3_column_double(stmt, 2);
    }
    if (sqlite3_column_type(stmt, 3) == SQLITE_NULL) {
        record.report.sog = std::nullopt;
    } else {
        record.report.sog = sqlite3_column_double(stmt, 3);
    }
    if (sqlite3_column_type(stmt, 4) == SQLITE_NULL) {
        record.report.cog = std::nullopt;
    } else {
        record.report.cog = sqlite3_column_double(stmt, 4);
    }
    if (sqlite3_column_type(stmt, 5) == SQLITE_NULL) {
        record.report.true_heading = std::nullopt;
    } else {
        record.report.true_heading = sqlite3_column_int(stmt, 5);
    }
    record.report.timestamp = sqlite3_column_int64(stmt, 6);
    record.report.message_type = sqlite3_column_int(stmt, 7);
    record.report.nav_status = static_cast<NavStatus>(sqlite3_column_int(stmt, 8));
    record.received_at = sqlite3_column_int64(stmt, 9);
    return record;
}
}