#include <gtest/gtest.h>

#include "ais/version.hpp"

// Scaffolding test: proves the library compiles, links, and runs under CTest.
// Delete this once you start writing real decoder tests in Fase 1.
TEST(Scaffold, LibraryLinks) {
    EXPECT_STREQ(ais::library_version(), "0.1.0");
}
