find_package(GTest CONFIG REQUIRED)

# One binary for all tests across modules, so a single debug launch (or a
# single `ctest` run) covers everything. Add new test .cpp files here.
add_executable(all_tests
    decoder_tests.cpp
    concurrent_queue_tests.cpp
    line_framer_tests.cpp
    tcp_client_tests.cpp
    tag_block_tests.cpp
    decoder_stage_tests.cpp
    sqlite_writer_tests.cpp
    position_reader_tests.cpp
    position_record_tests.cpp
    static_voyage_data_tests.cpp
    ais_message_tests.cpp
)

target_link_libraries(all_tests PRIVATE
    ais_decoder
    ais_pipeline
    ais_api
    GTest::gtest_main
)

target_compile_definitions(all_tests PRIVATE
    FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)

if(WIN32)
    target_link_libraries(all_tests PRIVATE ws2_32 mswsock)
endif()

# Registers each TEST() with CTest so `ctest` discovers them automatically.
include(GoogleTest)
gtest_discover_tests(all_tests)
