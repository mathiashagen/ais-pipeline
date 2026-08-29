#include "ais/line_framer.hpp"

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <algorithm>

TEST(LineFramer, SingleCompleteLine) {
    ais::LineFramer framer;
    auto lines = framer.feed("Hello, World!\n");
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "Hello, World!");
}

TEST(LineFramer, StripsCarriageReturns) {
    ais::LineFramer framer;
    auto lines = framer.feed("Hello, World!\r\r\n");
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "Hello, World!");
}

TEST(LineFramer, MultipleLinesInOneChunk) {
    ais::LineFramer framer;
    auto lines = framer.feed("Hello, World!\nGoodbye, World!\n");
    ASSERT_EQ(lines.size(), 2);
    EXPECT_EQ(lines[0], "Hello, World!");
    EXPECT_EQ(lines[1], "Goodbye, World!");
}

TEST(LineFramer, LineSplitAcrossFeeds) {
    ais::LineFramer framer;
    auto lines1 = framer.feed("Hello, ");
    ASSERT_EQ(lines1.size(), 0);
    auto lines2 = framer.feed("World!\n");
    ASSERT_EQ(lines2.size(), 1);
    EXPECT_EQ(lines2[0], "Hello, World!");
}

TEST(LineFramer, EmptyChunk) {
    ais::LineFramer framer;
    auto lines = framer.feed("");
    ASSERT_EQ(lines.size(), 0);
}

TEST(LineFramer, KystverketFixture) {
    std::ifstream file(std::string(FIXTURES_DIR) + "/kystverket_live_sample.nmea");
    ASSERT_TRUE(file.is_open());

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    ais::LineFramer framer;
    auto lines = framer.feed(content);

    ASSERT_EQ(lines.size(), 765);
    EXPECT_TRUE(std::none_of(lines.begin(), lines.end(), [](const std::string& line) {
        return line.find('\r') != std::string::npos;
    }));
}