# AIS-pipeline

[![CI](https://github.com/mathiashagen/ais-pipeline/actions/workflows/ci.yml/badge.svg)](https://github.com/mathiashagen/ais-pipeline/actions/workflows/ci.yml)

A real-time data pipeline in modern C++ that reads live AIS ship traffic from
Kystverket's open TCP feed, decodes the binary NMEA payloads bit by bit, stores
position reports and ship details in SQLite, and serves them over a small REST
API. A React radar display on top shows the ships around any point on the
Norwegian coast as they move.

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
│                 │  → AisMessage: position reports (types 1–3, 18)
│                 │    or static data (types 5, 24)
└────────┬────────┘
         │  ThreadSafeQueue<AisMessage>  (bounded; std::variant)
         ▼
┌─────────────────┐  writer thread
│  SqliteWriter   │  batched transactions, WAL mode,
│                 │  24-hour retention on position history
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
cooperative: SIGINT (Ctrl+C) or SIGTERM (`docker stop`) only sets a lock-free
`std::atomic<bool>`, since little else is safe inside a signal handler. `main`
sees it, stops the reader thread, closes each queue in order, and every
`std::jthread` drains and joins before `main` returns. `api_server` handles
the same signals by stopping its HTTP server and letting requests in progress
finish.

## What is implemented

- **AIVDM parsing**: checksum validation, field extraction, and the NMEA tag
  blocks Kystverket prefixes to each line. Multi-fragment messages are
  reassembled per source station, channel and sequence ID, and a broken
  fragment sequence is dropped rather than stitched together wrongly.
- **6-bit payload decoding** into a bitstream, with signed and unsigned field
  readers and 6-bit text fields.
- **Position reports**: Class A (message types 1, 2 and 3) and Class B
  (type 18). MMSI, position, speed and course over ground, heading,
  navigational status, and rate of turn for Class A.
- **Static data**: Class A static and voyage data (type 5: name, call sign,
  IMO, ship type, dimensions, draught, destination, ETA) and Class B static
  data (type 24, parts A and B). Every decoded message is an `AisMessage`, a
  `std::variant` the writer dispatches on with `std::visit`.
- **Pipeline**: Asio TCP client with connect and read timeouts, reconnect with
  exponential back-off and a prompt stop even mid-read; bounded thread-safe
  queues; and an SQLite writer using RAII wrappers around the C handles. It
  commits whatever has queued up as one transaction, so bursts are written in
  batches.
- **Storage**: every position report goes into `position_reports`, and a
  trigger keeps `latest_positions` at one row per ship. Static data is upserted
  into `ship_static`, one row per ship, with type 24 parts A and B filling in
  their own columns without overwriting each other. Position history is kept
  for 24 hours: every 5 minutes the writer deletes older reports in chunks of
  10,000, each its own transaction, so the write lock is never held for long.
  `latest_positions` and `ship_static` are not pruned.
- **REST API**: latest position per ship, ships inside a bounding box, and the
  history of one ship. Latest and area queries read `latest_positions` joined
  with `ship_static`, so they stay fast however much history accumulates;
  history uses an index on `(mmsi, received_at)`.
- **Radar display**: fixed on a centre chosen by place-name search, a click on
  the map or the device's position, with stepped ranges from 1 to 60 nm and
  range rings that always fill the view the same way. Ships are arrows along
  their heading, coloured by ship type, faded when they go quiet; selecting one
  shows its name, call sign, destination and status and draws its recorded
  track. The coastline is Kartverket's N250 data, pre-processed into tiles by
  `web/scripts/build-coastline.mjs`.
- **Tests**: 98 GoogleTest cases covering the decoder, sentence assembly,
  framer, tag blocks, queue, TCP client, writer and reader, including a
  fixture of 765 real lines captured from the live feed. Two of them connect
  to the live feed and are disabled by default.

## Throughput

Release build, on a Ryzen 7 9800X3D with a Samsung 990 PRO NVMe SSD.

### Decoder

Decoding the 765-line live fixture 200 times in a loop, single-threaded:

| Metric                                   | Value          |
| ---------------------------------------- | -------------- |
| Sentences parsed, assembled and decoded  | ~4.5 million/s |
| Time per sentence                        | ~0.22 µs       |

### Writer

Position reports pushed through a `ThreadSafeQueue` into `SqliteWriter::run()`
as fast as the queue accepts them, timed until the last one is committed. The
database has the full schema, so every insert also maintains the index and
the `latest_positions` trigger.

| Writer                                             | Reports/s  |
| -------------------------------------------------- | ---------- |
| One commit per report, `synchronous=FULL` (before) | ~670       |
| Batched transactions, 5,000-report burst           | ~580,000   |
| Batched transactions, sustained over 100,000       | ~125,000   |

Committing each report on its own made every insert wait for the disk to
confirm the write, so it barely changed between debug and release builds.
The writer now takes whatever has queued up since its last commit and writes
it as one transaction, with `synchronous=NORMAL`. On a quiet feed a batch is a
single report; batches grow only during bursts. The sustained figure is lower
than the burst because the database grows and SQLite periodically moves the
write-ahead log into the main file.

### In context

The live feed averages a few reports per second and has delivered around 650
in a single second. Before batching, a burst that size used the writer's
whole capacity. Now neither the decoder nor the writer comes close to being
the bottleneck; the feed itself is.

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

## Running with Docker

The quickest way to run everything is Docker Compose. From the repository
root:

```bash
docker compose up -d
```

Then open http://localhost:8080. The first start builds the images, which
takes a few minutes; the C++ build runs the test suite and stops if a test
fails.

| Service    | Image                                            | Role                                                        |
| ---------- | ------------------------------------------------ | ----------------------------------------------------------- |
| `pipeline` | `Dockerfile`, Ubuntu 24.04 runtime               | Reads the feed and writes `/data/ais_data.db`               |
| `api`      | Same image, `api_server` as the command          | Serves the REST API on port 8080 inside the Compose network |
| `web`      | `web/Dockerfile`, nginx                          | Serves the map and forwards `/api/` to `api`                |

Only `web` publishes a port. The browser calls the API as `/api/...` on the
same address, so there are no cross-origin requests; to use another host
port, change `"8080:80"` in `compose.yaml`. The API starts once a healthcheck
sees the pipeline's database file, since `api_server` exits if it is missing.
All three restart automatically unless you stop them.

```bash
docker compose logs -f          # follow the logs of all services
docker compose up -d --build    # rebuild and restart after changing the code
docker compose stop             # stop, keeping the containers
docker compose down             # remove containers and network, keep the data
```

The database lives in the named volume `ais-pipeline_ais-data`, so it
survives `down` and rebuilds. `docker compose down -v` deletes the volume and
with it all stored history.

Both images are multi-stage builds. The C++ image compiles on the same Ubuntu
24.04 base CI tests on and keeps only the two binaries and SQLite (about
120 MB), running as a non-root user. The web image builds with Node 22 and
serves the static files from `nginx:alpine`; the API base URL is baked in at
build time from the `VITE_API_URL` build argument, `/api` by default. Both
programs run as PID 1 and stop cleanly on the SIGTERM `docker stop` sends; CI
checks this for each.

## Running from a local build

Start the pipeline. It connects to Kystverket's feed and writes to
`ais_data.db` in the current directory, keeping the last 24 hours of position
history (roughly 190,000 reports on a normal day). Stop it with Ctrl+C:

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
seconds), plus `name`, `call_sign`, `destination` and `ship_type` from the
ship's static data. Fields the ship did not report are `null`; the static
fields are always `null` in history records. History goes back at most 24
hours. Bad query parameters give a 400 with a plain-text reason.

## Project layout

```
src/decoder/     Pure decoding library, no I/O: Sentence, Payload, SentenceAssembler,
                 PositionReport, StaticVoyageData, StaticDataReport, AisMessage
src/pipeline/    Runtime pieces: TcpClient, LineFramer, tag blocks, ThreadSafeQueue, DecoderStage, SqliteWriter
src/api/         PositionReader and the JSON shape of a record
src/app/         kystverket_pipeline executable
src/api_server/  api_server executable
tests/           GoogleTest suite and the live-capture fixture
third_party/     Vendored cpp-httplib (single header, not in pacman)
web/             React + Leaflet radar display (Vite, TypeScript), with its Dockerfile and nginx.conf
Dockerfile       Multi-stage image for kystverket_pipeline and api_server
compose.yaml     Runs pipeline, api and web together
```

## Dependencies

- [Asio](https://think-async.com/Asio/) (standalone) for TCP
- [SQLite](https://sqlite.org/) for storage
- [nlohmann/json](https://github.com/nlohmann/json) for the API responses
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) v0.54.1, vendored under `third_party/` (MIT)
- [GoogleTest](https://github.com/google/googletest) for the tests
- [React](https://react.dev/), [Leaflet](https://leafletjs.com/) and [react-leaflet](https://react-leaflet.js.org/) for the radar display
- [proj4js](https://github.com/proj4js/proj4js) to reproject the coastline data (build script only)

## Data and attribution

AIS data comes from [Kystverket](https://www.kystverket.no/sjotransport-og-havn/ais/tilgang-pa-ais-data/)'s
open feed, licensed under the Norwegian Licence for Open Government Data
(NLOD). The test fixture under `tests/fixtures/` is a capture of that feed and
is credited to Kystverket as their terms require.

The coastline under `web/public/coastline/` is derived from
[Kartverket](https://www.kartverket.no/)'s N250 Kartdata, © Kartverket,
licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). It
was reprojected to WGS84, simplified to 25 m and cut into 1° tiles; only the
coastline (`Kystkontur`) is kept. The radar display credits Kartverket on
screen wherever the coastline is shown.

Place-name search uses Kartverket's [Stedsnavn API](https://ws.geonorge.no/stedsnavn/v1/),
called from the browser.

## License

MIT, see [LICENSE](LICENSE).
