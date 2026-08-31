cmake_minimum_required(VERSION 3.25)
project(ais_pipeline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_subdirectory(third_party)
add_subdirectory(src/decoder)
add_subdirectory(src/pipeline)
add_subdirectory(tests)
add_subdirectory(src/app)
add_subdirectory(src/api)
