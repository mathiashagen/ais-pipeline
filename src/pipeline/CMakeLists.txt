add_library(ais_pipeline
    src/line_framer.cpp
    src/tcp_client.cpp
    src/tag_block.cpp
    src/decoder_stage.cpp
    src/sqlite_writer.cpp
)

target_link_libraries(ais_pipeline PUBLIC ais_decoder)

target_compile_definitions(ais_pipeline PUBLIC ASIO_STANDALONE)

# Standalone Asio needs Winsock on Windows; on Linux the sockets are in libc.
if(WIN32)
    target_compile_definitions(ais_pipeline PUBLIC _WIN32_WINNT=0x0A00)
    target_link_libraries(ais_pipeline PUBLIC ws2_32 mswsock)
endif()

find_package(SQLite3 REQUIRED)
target_link_libraries(ais_pipeline PUBLIC SQLite3::SQLite3)

target_include_directories(ais_pipeline PUBLIC include)
target_compile_features(ais_pipeline PUBLIC cxx_std_20)