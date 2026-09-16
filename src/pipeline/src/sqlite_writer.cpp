#include "ais/sqlite_writer.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ais {

namespace {

template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

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

// Bind an optional field, or NULL when absent. Two overloads rather than one
// template: the compiler picks by the optional's type, exactly as C# overload
// resolution would, and neither body has to cope with the other's type.
void bind_optional(sqlite3_stmt* stmt, int index, const std::optional<double>& value) {
    if (value.has_value()) {
        sqlite3_bind_double(stmt, index, *value);
    } else {
        sqlite3_bind_null(stmt, index);
    }
}

void bind_optional(sqlite3_stmt* stmt, int index, const std::optional<int>& value) {
    if (value.has_value()) {
        sqlite3_bind_int(stmt, index, *value);
    } else {
        sqlite3_bind_null(stmt, index);
    }
}

void bind_optional(sqlite3_stmt* stmt, int index, const std::optional<std::uint8_t>& value) {
    if (value.has_value()) {
        sqlite3_bind_int(stmt, index, *value);
    } else {
        sqlite3_bind_null(stmt, index);
    }
}

void bind_optional(sqlite3_stmt* stmt, int index, const std::optional<std::uint32_t>& value) {
    if (value.has_value()) {
        sqlite3_bind_int(stmt, index, *value);
    } else {
        sqlite3_bind_null(stmt, index);
    }
}

void bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
    if (value.empty()) {
        sqlite3_bind_null(stmt, index);
    } else {
        sqlite3_bind_text(stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_STATIC);
    }
}

void step_once(sqlite3* db, sqlite3_stmt* stmt) {
    const int result = sqlite3_step(stmt);
    const std::string error = result == SQLITE_DONE ? std::string() : sqlite3_errmsg(db);
    sqlite3_reset(stmt);
    if (result != SQLITE_DONE) {
        throw std::runtime_error(error);
    }
}

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

    // FULL, the default, waits for the disk to confirm every commit. In WAL
    // mode NORMAL is still crash-safe -- the database is never corrupted, and
    // a crash of this process loses nothing -- but a power cut or OS crash can
    // roll back the last few commits. For positions that are replaced within
    // seconds anyway, that is worth the throughput.
    //
    // Per connection, unlike journal_mode: nothing about it is stored in the
    // file, so it has to be set every time the database is opened.
    exec(connection_.get(), "PRAGMA synchronous=NORMAL;");

    const char* sql = 
        "CREATE TABLE IF NOT EXISTS "
        "position_reports (mmsi INTEGER, latitude REAL, longitude REAL, sog REAL, cog REAL, true_heading REAL, timestamp INTEGER, message_type INTEGER, nav_status INTEGER, received_at INTEGER); "
        "CREATE TABLE IF NOT EXISTS ship_static ("
        "mmsi INTEGER PRIMARY KEY,"
        "imo INTEGER, call_sign TEXT, name TEXT, ship_type INTEGER,"
        "to_bow INTEGER, to_stern INTEGER, to_port INTEGER, to_starboard INTEGER,"
        "eta_month INTEGER, eta_day INTEGER, eta_hour INTEGER, eta_minute INTEGER,"
        "draught REAL, destination TEXT, received_at INTEGER);";

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

    stmt = nullptr;
    const char* static_insert_sql =
        "INSERT INTO ship_static (mmsi, imo, call_sign, name, ship_type, to_bow, to_stern, to_port, to_starboard, eta_month, eta_day, eta_hour, eta_minute, draught, destination, received_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT (mmsi) DO UPDATE SET imo = excluded.imo, call_sign = excluded.call_sign, name = excluded.name, ship_type = excluded.ship_type, to_bow = excluded.to_bow, to_stern = excluded.to_stern, to_port = excluded.to_port, to_starboard = excluded.to_starboard, eta_month = excluded.eta_month, eta_day = excluded.eta_day, eta_hour = excluded.eta_hour, eta_minute = excluded.eta_minute, draught = excluded.draught, destination = excluded.destination, received_at = excluded.received_at "
        "WHERE excluded.received_at >= ship_static.received_at;";
    if (sqlite3_prepare_v2(connection_.get(), static_insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(connection_.get()));
    }
    static_statement_ = SqliteStatement(stmt);

    stmt = nullptr;
    const char* static_data_part_a_insert_sql =
        "INSERT INTO ship_static (mmsi, name, received_at) "
        "VALUES (?, ?, ?) "
        "ON CONFLICT (mmsi) DO UPDATE SET name = excluded.name, received_at = excluded.received_at "
        "WHERE excluded.received_at >= ship_static.received_at;";
    if (sqlite3_prepare_v2(connection_.get(), static_data_part_a_insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(connection_.get()));
    }
    part_a_statement_ = SqliteStatement(stmt);

    stmt = nullptr;
    const char* static_data_part_b_insert_sql =
        "INSERT INTO ship_static (mmsi, ship_type, call_sign, to_bow, to_stern, to_port, to_starboard, received_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT (mmsi) DO UPDATE SET ship_type = excluded.ship_type, call_sign = excluded.call_sign, to_bow = excluded.to_bow, to_stern = excluded.to_stern, to_port = excluded.to_port, to_starboard = excluded.to_starboard, received_at = excluded.received_at "
        "WHERE excluded.received_at >= ship_static.received_at;";
    if (sqlite3_prepare_v2(connection_.get(), static_data_part_b_insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(connection_.get()));
    }
    part_b_statement_ = SqliteStatement(stmt);
}

// Writes whatever has queued up since the last commit as one transaction.
//
// Committing each report on its own made every insert wait for the disk: the
// writer topped out near 700 reports/s, about the size of the feed's bursts.
// A batch costs one commit however many reports it holds. pop_batch does not
// wait to fill a batch, so a quiet feed still commits each report as it
// arrives, and batches only grow when reports arrive faster than one commit.
//
// If an insert fails, the Transaction rolls the whole batch back as it unwinds,
// so the database never holds part of one. The exception then leaves run(),
// and an exception escaping a std::jthread's function calls std::terminate --
// the process stops, as it did before batching. Unlike an unobserved exception
// in a C# Task, it cannot be silently lost.
void SqliteWriter::run(ThreadSafeQueue<AisMessage>& input, std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::vector<AisMessage> batch = input.pop_batch(max_batch_size);
        if (batch.empty()) {
            break;  // closed and drained: the last reports were committed already
        }

        // One timestamp for the batch. received_at has whole-second resolution,
        // and a batch is written within milliseconds; same-second reports are
        // ordered by insertion, which the loop below preserves.
        const std::int64_t received_at = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        Transaction transaction(connection_.get());
        for (const AisMessage& message : batch) {
            std::visit(overloaded{
                [&](const PositionReport& report) { insert(report, received_at); },
                [&](const StaticVoyageData& static_data) { upsert(static_data, received_at); },
                [&](const StaticDataPartA& static_data_a) { upsert(static_data_a, received_at); },
                [&](const StaticDataPartB& static_data_b) { upsert(static_data_b, received_at); },
            }, message);
        }
        transaction.commit();

        const auto now = std::chrono::steady_clock::now();
        if (now - last_prune_ >= prune_interval) {
            const auto cutoff = std::chrono::duration_cast<std::chrono::seconds>(
                (std::chrono::system_clock::now() - retention).time_since_epoch()).count();
            delete_older_than(cutoff);
            last_prune_ = now;
        }
    }
}

void SqliteWriter::insert(const PositionReport& report, std::int64_t received_at) {
    sqlite3_stmt* stmt = statement_.get();

    sqlite3_bind_int(stmt, 1, report.mmsi);
    bind_optional(stmt, 2, report.latitude);
    bind_optional(stmt, 3, report.longitude);
    bind_optional(stmt, 4, report.sog);
    bind_optional(stmt, 5, report.cog);
    bind_optional(stmt, 6, report.true_heading);
    sqlite3_bind_int(stmt, 7, report.timestamp);
    sqlite3_bind_int(stmt, 8, report.message_type);
    sqlite3_bind_int(stmt, 9, static_cast<int>(report.nav_status));
    sqlite3_bind_int64(stmt, 10, received_at);

    step_once(connection_.get(), stmt);
}

void SqliteWriter::upsert(const StaticVoyageData& static_data, std::int64_t received_at) {
    sqlite3_stmt* stmt = static_statement_.get();

    sqlite3_bind_int(stmt, 1, static_data.mmsi);
    bind_optional(stmt, 2, static_data.imo);
    bind_text(stmt, 3, static_data.call_sign);
    bind_text(stmt, 4, static_data.name);
    sqlite3_bind_int(stmt, 5, static_data.ship_type);
    sqlite3_bind_int(stmt, 6, static_data.to_bow);
    sqlite3_bind_int(stmt, 7, static_data.to_stern);
    sqlite3_bind_int(stmt, 8, static_data.to_port);
    sqlite3_bind_int(stmt, 9, static_data.to_starboard);
    bind_optional(stmt, 10, static_data.eta_month);
    bind_optional(stmt, 11, static_data.eta_day);
    bind_optional(stmt, 12, static_data.eta_hour);
    bind_optional(stmt, 13, static_data.eta_minute);
    bind_optional(stmt, 14, static_data.draught);
    bind_text(stmt, 15, static_data.destination);
    sqlite3_bind_int64(stmt, 16, received_at);

    step_once(connection_.get(), stmt);
}

void SqliteWriter::upsert(const StaticDataPartA& static_data_a, std::int64_t received_at) {
    sqlite3_stmt* stmt = part_a_statement_.get();

    sqlite3_bind_int(stmt, 1, static_data_a.mmsi);
    bind_text(stmt, 2, static_data_a.name);
    sqlite3_bind_int64(stmt, 3, received_at);

    step_once(connection_.get(), stmt);
}

void SqliteWriter::upsert(const StaticDataPartB& static_data_b, std::int64_t received_at) {
    sqlite3_stmt* stmt = part_b_statement_.get();

    sqlite3_bind_int(stmt, 1, static_data_b.mmsi);
    sqlite3_bind_int(stmt, 2, static_data_b.ship_type);
    bind_text(stmt, 3, static_data_b.call_sign);
    sqlite3_bind_int(stmt, 4, static_data_b.to_bow);
    sqlite3_bind_int(stmt, 5, static_data_b.to_stern);
    sqlite3_bind_int(stmt, 6, static_data_b.to_port);
    sqlite3_bind_int(stmt, 7, static_data_b.to_starboard);
    sqlite3_bind_int64(stmt, 8, received_at);

    step_once(connection_.get(), stmt);
}

std::size_t SqliteWriter::delete_older_than(std::int64_t cutoff) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM position_reports WHERE rowid IN (SELECT rowid FROM position_reports WHERE received_at < ? LIMIT ?)";
    if (sqlite3_prepare_v2(connection_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(connection_.get()));
    }
    SqliteStatement statement(stmt);
    sqlite3_bind_int64(stmt, 1, cutoff);
    sqlite3_bind_int64(stmt, 2, static_cast<std::int64_t>(prune_chunk_size));
    std::size_t total = 0;
    std::size_t deleted = 0;
    do {
        step_once(connection_.get(), stmt);
        deleted = static_cast<std::size_t>(sqlite3_changes(connection_.get()));
        total += deleted;
    } while (deleted == prune_chunk_size);

    return total;
}

}