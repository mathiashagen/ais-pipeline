# AIS-pipeline

[![CI](https://github.com/GITHUB_USER/AIS-pipeline/actions/workflows/ci.yml/badge.svg)](https://github.com/GITHUB_USER/AIS-pipeline/actions/workflows/ci.yml)

A real-time data pipeline in modern C++ that reads live AIS ship traffic from
Kystverket's open TCP feed, decodes the binary NMEA payloads bit by bit, stores
every position report in SQLite, and serves them over a small REST API. A React
map on top shows the ships around Ålesund as they move.

The map is the least interesting part. The point of the project is the plumbing
underneath: decoding a binary protocol at the bit level, keeping a long-running
network connection healthy, and moving data safely across three threads.

I built it to learn C++20 properly, coming from C#/.NET and TypeScript. Design
notes in the code explain *why* things are done a certain way, often in
contrast to how the same thing would look in C#.

## Architecture

```
Kystverket AIS feed  (TCP, NMEA 0183 sentences with tag blocks)
        │
        ▼
┌─────────────────┐  reader thread
│    TcpClient    │  Asio socket; LineFramer turns byte chunks into whole lines,
│                 │  exponential back-off on reconnect
└────────┬────────┘
         │  ThreadSafeQueue<std::string>  (bounded)
         ▼
┌─────────────────┐  decoder thread
│  DecoderStage   │  strip tag block → Sentence (checksum, fields)
│                 │  → SentenceAssembler (multi-part messages)
│                 │  → Payload (6-bit "armored" ASCII → bitstream)
│                 │  → PositionReport (message types 1–3)
└────────┬────────┘
         │  ThreadSafeQueue<PositionReport>  (bounded)
         ▼
┌─────────────────┐  writer thread
│  SqliteWriter   │  prepared INSERT, WAL mode
└────────┬────────┘
         │  ais_data.db
         ▼
┌─────────────────┐         ┌──────────────────┐
│   api_server    │  HTTP   │  web/            │
│  cpp-httplib +  │ ──────▶ │  React + Leaflet │
│  PositionReader │  JSON   │                  │
└─────────────────┘         └──────────────────┘
```

Two processes share the database: `kystverket_pipeline` writes, `api_server`
reads. SQLite's WAL mode is what lets them do that concurrently without the
reader blocking the writer.

The queues between stages are bounded, so a slow SQLite write pushes back on
the decoder rather than letting memory grow without limit. Shutdown is
cooperative: Ctrl+C requests a stop through a `std::stop_source`, each queue is
closed in order, and every `std::jthread` drains and joins before `main`
returns.

## What is implemented

- **AIVDM parsing**: checksum validation, field extraction, multi-fragment
  reassembly, and the NMEA tag blocks Kystverket prefixes to each line.
- **6-bit payload decoding** into a bitstream, with signed and unsigned field
  readers.
- **Position reports** (message types 1, 2 and 3): MMSI, position, speed and
  course over ground, heading, navigational status, rate of turn.
- **Pipeline**: Asio TCP client with reconnect, a bounded thread-safe queue,
  and an SQLite writer using RAII wrappers around the C handles.
- **REST API**: latest position per ship, ships inside a bounding box, and the
  history of one ship.
- **Map**: live markers that fade when a ship goes quiet, and the recorded
  track of a selected ship.
- **Tests**: 35 GoogleTest cases covering the decoder, framer, queue, TCP
  client, writer and reader, including a fixture of 765 real lines captured
  from the live feed.

Static ship data (message type 5, with name and dimensions) is not decoded yet.

## Decoder throughput

Decoding the 765-line live fixture 200 times in a loop, single-threaded,
release build, on a Ryzen 7 9800X3D:

| Metric                                   | Value          |
| ---------------------------------------- | -------------- |
| Sentences parsed, assembled and decoded  | ~4.5 million/s |
| Time per sentence                        | ~0.22 µs       |

The live feed delivers a few hundred sentences per second, so the decoder is
nowhere near the bottleneck; the SQLite write is.

## Building

The project is developed on Windows with GCC from MSYS2 UCRT64 and also builds
on Linux (CI runs the sanitizer build there). C++20 and CMake 3.25+ are
required.

### Windows (MSYS2 UCRT64)

Install the toolchain and libraries from the "MSYS2 UCRT64" shell:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-gtest mingw-w64-ucrt-x86_64-asio \
          mingw-w64-ucrt-x86_64-nlohmann-json mingw-w64-ucrt-x86_64-sqlite3
```

### Linux (Debian/Ubuntu)

```bash
sudo apt-get install build-essential cmake ninja-build libgtest-dev libasio-dev nlohmann-json3-dev libsqlite3-dev
```

### Configure, build, test

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Presets: `debug`, `release`, and `asan` (AddressSanitizer + UBSan, Linux only,
since MinGW GCC ships no libasan). The `.vscode/` folder configures CMake
Tools, clangd and gdb for the MSYS2 layout.

## Running

Start the pipeline. It connects to Kystverket's feed and writes to
`ais_data.db` in the current directory:

```bash
./build/debug/src/app/kystverket_pipeline [host] [port] [db_path]
```

Start the API in a second shell:

```bash
./build/debug/src/api_server/api_server [db_path] [port]
```

Then the map, which expects the API on port 8080 (override with
`VITE_API_URL`, see `web/.env.example`):

```bash
cd web && npm install && npm run dev
```

## API

| Endpoint                                           | Returns                                              |
| -------------------------------------------------- | ---------------------------------------------------- |
| `GET /positions`                                   | Latest position per ship.                            |
| `GET /positions/area?min_lat&max_lat&min_lon&max_lon` | Latest position per ship inside the box.          |
| `GET /positions/{mmsi}/history?limit=50`           | Recent reports for one ship, newest first, limit 1–1000. |

Every record is a JSON object with `mmsi`, `latitude`, `longitude`, `sog`,
`cog`, `true_heading`, `nav_status`, `timestamp` and `received_at` (Unix
seconds). Fields the ship did not report are `null`. Bad query parameters give
a 400 with a plain-text reason.

## Project layout

```
src/decoder/     Pure decoding library, no I/O: Sentence, Payload, SentenceAssembler, PositionReport
src/pipeline/    Runtime pieces: TcpClient, LineFramer, tag blocks, ThreadSafeQueue, DecoderStage, SqliteWriter
src/api/         PositionReader and the JSON shape of a record
src/app/         kystverket_pipeline executable
src/api_server/  api_server executable
tests/           GoogleTest suite and the live-capture fixture
third_party/     Vendored cpp-httplib (single header, not in pacman)
web/             React + Leaflet map (Vite, TypeScript)
```

## Dependencies

- [Asio](https://think-async.com/Asio/) (standalone) for TCP
- [SQLite](https://sqlite.org/) for storage
- [nlohmann/json](https://github.com/nlohmann/json) for the API responses
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) v0.54.1, vendored under `third_party/` (MIT)
- [GoogleTest](https://github.com/google/googletest) for the tests
- [React](https://react.dev/), [Leaflet](https://leafletjs.com/) and [react-leaflet](https://react-leaflet.js.org/) for the map

## Data and attribution

AIS data comes from [Kystverket](https://www.kystverket.no/sjotransport-og-havn/ais/tilgang-pa-ais-data/)'s
open feed, licensed under the Norwegian Licence for Open Government Data
(NLOD). The test fixture under `tests/fixtures/` is a capture of that feed and
is credited to Kystverket as their terms require. Map tiles are from
[OpenStreetMap](https://www.openstreetmap.org/copyright) contributors.

## License

MIT, see [LICENSE](LICENSE).
