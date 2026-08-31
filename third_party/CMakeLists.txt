# cpp-httplib is a single header, not packaged by MSYS2/pacman, so it's
# vendored here directly (pinned to release v0.54.1) rather than installed
# as a system dependency.
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE cpp-httplib)
