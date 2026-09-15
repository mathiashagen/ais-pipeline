#pragma once

#include "ais/concurrent_queue.hpp"
#include "ais/position_report.hpp"
#include "ais/ais_message.hpp"
#include "ais/static_voyage_data.hpp"


#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <stop_token>
#include <sqlite3.h>

namespace ais {

struct Sqlite3CloseDeleter {
    void operator()(sqlite3* db) const { sqlite3_close(db); }
};

struct Sqlite3FinalizeDeleter {
    void operator()(sqlite3_stmt* stmt) const { sqlite3_finalize(stmt); }
};

using SqliteConnection = std::unique_ptr<sqlite3, Sqlite3CloseDeleter>;
using SqliteStatement = std::unique_ptr<sqlite3_stmt, Sqlite3FinalizeDeleter>;

class SqliteWriter {
public:
    /**
     * Most reports written in one transaction. Bounds how long a single
     * commit holds the write lock during a burst; matches the pipeline's
     * queue capacity, so one batch can empty a full queue.
     */
    static constexpr std::size_t max_batch_size = 500;

    explicit SqliteWriter(std::string db_path);
    void run(ThreadSafeQueue<AisMessage>& input, std::stop_token stop_token);
private:
    void insert(const PositionReport& report, std::int64_t received_at);
    void upsert(const StaticVoyageData& static_data, std::int64_t received_at);

    SqliteConnection connection_;
    SqliteStatement statement_;
    SqliteStatement static_statement_;
};

}