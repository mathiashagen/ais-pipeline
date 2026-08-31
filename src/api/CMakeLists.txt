add_library(ais_api
    src/position_reader.cpp
)

target_include_directories(ais_api PUBLIC include)

target_link_libraries(ais_api PUBLIC ais_pipeline ais_decoder)