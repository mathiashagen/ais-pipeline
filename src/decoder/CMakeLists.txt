# The core decoder library (Fase 1). For now it only carries a placeholder
# so the toolchain can be verified end-to-end. You replace this with the real
# AIVDM parsing/decoding API.
add_library(ais_decoder
    src/version.cpp
)

target_include_directories(ais_decoder PUBLIC include)
target_compile_features(ais_decoder PUBLIC cxx_std_20)
