add_executable(api_server main.cpp)

target_link_libraries(api_server PRIVATE ais_api httplib)