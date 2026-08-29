find_package(GTest CONFIG REQUIRED)

# One binary for all tests across modules, so a single debug launch (or a
# single `ctest` run) covers everything. Add new test .cpp files here.
add_executable(all_tests
    decoder_tests.cpp
    concurrent_queue_tests.cpp
    line_framer_tests.cpp
    tcp_client_tests.cpp
)

target_link_libraries(all_tests PRIVATE
    ais_decoder
    ais_pipeline
    GTest::gtest_main
)

target_compile_definitions(all_tests PRIVATE
    FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)

target_compile_definitions(all_tests PRIVATE ASIO_STANDALONE _WIN32_WINNT=0x0601)
target_link_libraries(all_tests PRIVATE ws2_32 mswsock)

# Registers each TEST() with CTest so `ctest` discovers them automatically.
include(GoogleTest)
gtest_discover_tests(all_tests)
