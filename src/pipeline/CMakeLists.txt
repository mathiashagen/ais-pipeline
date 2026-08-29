add_library(ais_pipeline
    src/line_framer.cpp
    src/tcp_client.cpp
)

target_compile_definitions(ais_pipeline PUBLIC ASIO_STANDALONE _WIN32_WINNT=0x0601)
target_link_libraries(ais_pipeline PUBLIC ws2_32 mswsock)

target_include_directories(ais_pipeline PUBLIC include)
target_compile_features(ais_pipeline PUBLIC cxx_std_20)