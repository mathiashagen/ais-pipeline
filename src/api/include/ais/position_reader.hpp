#pragma once

#include "ais/sqlite_writer.hpp"
#include "ais/position_record.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

namespace ais {

struct BoundingBox {
    double min_lat, max_lat, min_lon, max_lon;
};

class PositionReader
{

public:
    explicit PositionReader(std::string db_path);
    std::vector<PositionRecord> history(std::uint32_t mmsi, int limit);
    std::vector<PositionRecord> latest_positions();
    std::vector<PositionRecord> positions_in_area(BoundingBox box);

private:
    ais::SqliteConnection connection_;
    ais::SqliteStatement history_statement_;
    ais::SqliteStatement latest_positions_statement_;
    ais::SqliteStatement positions_in_area_statement_;
    PositionRecord read_current_row(sqlite3_stmt* stmt);
    std::vector<PositionRecord> collect_rows(sqlite3_stmt* stmt);
    std::mutex mutex_;
};

}