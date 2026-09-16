FROM ubuntu:24.04 AS build
RUN  apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build libgtest-dev libasio-dev nlohmann-json3-dev libsqlite3-dev && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY src/ ./src/
COPY tests/ ./tests/
COPY third_party/ ./third_party/
COPY CMakeLists.txt CMakePresets.json ./
RUN cmake --preset release && cmake --build --preset release
RUN ctest --test-dir build/release --output-on-failure --no-tests=error

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libsqlite3-0 && \
    rm -rf /var/lib/apt/lists/*
RUN useradd --system --user-group --no-create-home --shell /usr/sbin/nologin ais && \
    mkdir /data && chown ais:ais /data
COPY --from=build /app/build/release/src/app/kystverket_pipeline \
                  /app/build/release/src/api_server/api_server \
                  /usr/local/bin/
USER ais
WORKDIR /data
VOLUME /data
EXPOSE 8080
CMD ["kystverket_pipeline"]