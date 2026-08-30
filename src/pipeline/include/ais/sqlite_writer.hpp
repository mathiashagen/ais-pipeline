#pragma once

#include "ais/concurrent_queue.hpp"
#include "ais/position_report.hpp"

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
    explicit SqliteWriter(std::string db_path);
    void run(ThreadSafeQueue<PositionReport>& input, std::stop_token stop_token);
private:
    SqliteConnection connection_;
    SqliteStatement statement_;
};

}