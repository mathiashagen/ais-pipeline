#include "ais/sqlite_writer.hpp"

#include <chrono>

namespace ais {

SqliteWriter::SqliteWriter(std::string db_path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    connection_ = SqliteConnection(db);

    const char* sql = 
        "CREATE TABLE IF NOT EXISTS "
        "position_reports (mmsi INTEGER, latitude REAL, longitude REAL, sog REAL, cog REAL, true_heading REAL, timestamp INTEGER, message_type INTEGER, nav_status INTEGER, received_at INTEGER); ";

    if (sqlite3_exec(connection_.get(), sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(connection_.get()));
    }

    sqlite3_stmt* stmt = nullptr;
    const char* insert_sql = 
        "INSERT INTO position_reports (mmsi, latitude, longitude, sog, cog, true_heading, timestamp, message_type, nav_status, received_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(connection_.get(), insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(connection_.get()));
    }
    statement_ = SqliteStatement(stmt);
}

void SqliteWriter::run(ThreadSafeQueue<PositionReport>& input, std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::optional<PositionReport> report = input.pop();
        if (report) {
            // Bind values to the prepared statement and execute it
            sqlite3_stmt* stmt = statement_.get();
            sqlite3_reset(stmt);
            sqlite3_bind_int(stmt, 1, report->mmsi);
            if (report->latitude.has_value()) {
                sqlite3_bind_double(stmt, 2, report->latitude.value());
            } else {
                sqlite3_bind_null(stmt, 2);
            }
            if (report->longitude.has_value()) {
                sqlite3_bind_double(stmt, 3, report->longitude.value());
            } else {
                sqlite3_bind_null(stmt, 3);
            }
            if (report->sog.has_value()) {
                sqlite3_bind_double(stmt, 4, report->sog.value());
            } else {
                sqlite3_bind_null(stmt, 4);
            }
            if (report->cog.has_value()) {
                sqlite3_bind_double(stmt, 5, report->cog.value());
            } else {
                sqlite3_bind_null(stmt, 5);
            }
            if (report->true_heading.has_value()) {
                sqlite3_bind_int(stmt, 6, report->true_heading.value());
            } else {
                sqlite3_bind_null(stmt, 6);
            }
            sqlite3_bind_int(stmt, 7, report->timestamp);
            sqlite3_bind_int(stmt, 8, report->message_type);
            sqlite3_bind_int(stmt, 9, static_cast<int>(report->nav_status));
            auto now = std::chrono::system_clock::now();
            sqlite3_bind_int64(stmt, 10, std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                throw std::runtime_error(sqlite3_errmsg(connection_.get()));
            }
        }
        else {
            break;
        }
    }
}

}