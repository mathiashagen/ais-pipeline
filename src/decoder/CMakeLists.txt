# The core decoder library (Fase 1). Building up the real AIVDM parsing/
# decoding API one slice at a time. version.cpp is the original scaffold and
# can be removed once a real public API exists.
add_library(ais_decoder
    src/version.cpp
    src/sentence.cpp
    src/payload.cpp
    src/position_report.cpp
    src/decoder.cpp
    src/sentence_assembler.cpp
    src/static_voyage_data.cpp
    src/ais_message.cpp
    src/static_data_report.cpp
)

target_include_directories(ais_decoder PUBLIC include)
target_compile_features(ais_decoder PUBLIC cxx_std_20)
