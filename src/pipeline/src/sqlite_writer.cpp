#include "ais/sqlite_writer.hpp"

#include <chrono>
#include <string_view>

namespace ais {

namespace {

void exec(sqlite3* db, const char* sql) {
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

// A write transaction that rolls back unless commit() is reached. If anything
// between BEGIN and commit() throws, unwinding runs the destructor and undoes
// the partial work -- the same guarantee as C#'s `using var tx = ...` without
// a Commit(), but coming from the destructor rather than Dispose.
class Transaction {
public:
    explicit Transaction(sqlite3* db) : db_(db) {
        // IMMEDIATE takes the write lock now rather than at the first write,
        // so a busy database fails here, before any work is done.
        exec(db_, "BEGIN IMMEDIATE");
    }

    ~Transaction() {
        if (db_ != nullptr) {
            // Destructors must not throw; a failed rollback leaves nothing
            // more that could be done here anyway.
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        }
    }

    void commit() {
        exec(db_, "COMMIT");
        db_ = nullptr;  // only after COMMIT succeeded; otherwise still roll back
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

private:
    sqlite3* db_;
};

// Runs a query returning a single 0/1 column, such as SELECT EXISTS (...).
bool has_rows(sqlite3* db, const char* exists_query) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, exists_query, -1, &raw, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    SqliteStatement statement(raw);
    if (sqlite3_step(raw) != SQLITE_ROW) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    return sqlite3_column_int(raw, 0) != 0;
}

// One row per ship: its most recent report that carried a position. Kept up to
// date by the trigger below rather than by run(), so every insert into
// position_reports maintains it -- the pipeline, the tests' raw inserts, a
// manual fix in the sqlite3 shell -- and it can never disagree with history.
//
// mmsi is INTEGER PRIMARY KEY, which in SQLite makes it the table's rowid:
// lookups by mmsi need no separate index.
const char* const latest_positions_table_sql = R"sql(
    CREATE TABLE IF NOT EXISTS latest_positions (
        mmsi INTEGER PRIMARY KEY,
        latitude REAL, longitude REAL, sog REAL, cog REAL, true_heading REAL,
        timestamp INTEGER, message_type INTEGER, nav_status INTEGER, received_at INTEGER
    );
)sql";

// Runs inside the INSERT statement that fires it, so the two tables change
// together or not at all.
//
// WHEN: a report without a position leaves the ship's last known position in
//   place, so it stays on the radar instead of vanishing.
// WHERE on the update: an older report arriving late does not overwrite a
//   newer one. >= rather than >: received_at has whole-second resolution and
//   same-second reports are common; the one stored last wins, as rowid order
//   decided in the query this replaces.
const char* const latest_positions_trigger_sql = R"sql(
    CREATE TRIGGER IF NOT EXISTS position_reports_update_latest
    AFTER INSERT ON position_reports
    WHEN NEW.mmsi IS NOT NULL AND NEW.latitude IS NOT NULL AND NEW.longitude IS NOT NULL
    BEGIN
        INSERT INTO latest_positions (mmsi, latitude, longitude, sog, cog, true_heading,
                                      timestamp, message_type, nav_status, received_at)
        VALUES (NEW.mmsi, NEW.latitude, NEW.longitude, NEW.sog, NEW.cog, NEW.true_heading,
                NEW.timestamp, NEW.message_type, NEW.nav_status, NEW.received_at)
        ON CONFLICT (mmsi) DO UPDATE SET
            latitude = excluded.latitude,
            longitude = excluded.longitude,
            sog = excluded.sog,
            cog = excluded.cog,
            true_heading = excluded.true_heading,
            timestamp = excluded.timestamp,
            message_type = excluded.message_type,
            nav_status = excluded.nav_status,
            received_at = excluded.received_at
        WHERE excluded.received_at >= latest_positions.received_at;
    END;
)sql";

// Fills latest_positions from history, for a database created before the
// table existed. Rows without a position are dropped *before* ranking: ranked
// first, a ship whose very newest report lacks a position would be picked on
// that row, then filtered out, and lost -- rather than getting its last
// report that had one, which is what the trigger keeps for new data.
const char* const latest_positions_backfill_sql = R"sql(
    INSERT INTO latest_positions (mmsi, latitude, longitude, sog, cog, true_heading,
                                  timestamp, message_type, nav_status, received_at)
    SELECT mmsi, latitude, longitude, sog, cog, true_heading,
           timestamp, message_type, nav_status, received_at
    FROM (
        SELECT *, ROW_NUMBER() OVER (PARTITION BY mmsi ORDER BY received_at DESC, rowid DESC) AS rn
        FROM position_reports
        WHERE mmsi IS NOT NULL AND latitude IS NOT NULL AND longitude IS NOT NULL
    )
    WHERE rn = 1;
)sql";

}  // namespace

SqliteWriter::SqliteWriter(std::string db_path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    connection_ = SqliteConnection(db);
    sqlite3_busy_timeout(connection_.get(), 5000);

    {
        // Scoped so the statement is finalized as soon as the check is done. A
        // statement that has returned a row stays active until it is reset or
        // finalized, and SQLite refuses to COMMIT while any statement on the
        // connection is still active -- which the transaction below needs.
        sqlite3_stmt* wal_stmt = nullptr;
        if (sqlite3_prepare_v2(connection_.get(), "PRAGMA journal_mode=WAL;", -1, &wal_stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(connection_.get()));
        }
        SqliteStatement wal_statement(wal_stmt);

        if (sqlite3_step(wal_stmt) != SQLITE_ROW) {
            throw std::runtime_error(sqlite3_errmsg(connection_.get()));
        }

        const unsigned char* mode = sqlite3_column_text(wal_stmt, 0);
        if (mode == nullptr || std::string_view(reinterpret_cast<const char*>(mode)) != "wal") {
            throw std::runtime_error("could not enable WAL mode on " + db_path);
        }
    }

    const char* sql = 
        "CREATE TABLE IF NOT EXISTS "
        "position_reports (mmsi INTEGER, latitude REAL, longitude REAL, sog REAL, cog REAL, true_heading REAL, timestamp INTEGER, message_type INTEGER, nav_status INTEGER, received_at INTEGER); ";

    if (sqlite3_exec(connection_.get(), sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(connection_.get()));
    }

    const char* index_sql = 
        "CREATE INDEX IF NOT EXISTS "
        "position_reports_mmsi_received_at_idx ON position_reports (mmsi, received_at);";
    if (sqlite3_exec(connection_.get(), index_sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(connection_.get()));
    }

    {
        // Table, trigger and backfill as one unit. Without the transaction, a
        // crash after the trigger exists but before the backfill ran would
        // leave latest_positions with only the ships reported since -- and
        // since the table is then no longer empty, the next start would never
        // backfill the rest.
        Transaction transaction(connection_.get());
        exec(connection_.get(), latest_positions_table_sql);
        exec(connection_.get(), latest_positions_trigger_sql);
        if (!has_rows(connection_.get(), "SELECT EXISTS (SELECT 1 FROM latest_positions)")) {
            exec(connection_.get(), latest_positions_backfill_sql);
        }
        transaction.commit();
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