find_package(GTest CONFIG REQUIRED)

add_executable(decoder_tests
    decoder_tests.cpp
)

target_link_libraries(decoder_tests PRIVATE
    ais_decoder
    GTest::gtest_main
)

# Registers each TEST() with CTest so `ctest` discovers them automatically.
include(GoogleTest)
gtest_discover_tests(decoder_tests)
