#include "ais/tag_block.hpp"
#include <gtest/gtest.h>

TEST(TagBlockTests, StripTagBlock) {
    using ais::strip_tag_block;

    EXPECT_EQ(strip_tag_block("\\tag\\content"), std::optional<std::string_view>("content"));
    EXPECT_EQ(strip_tag_block("\\tag\\"), std::optional<std::string_view>(""));
    EXPECT_EQ(strip_tag_block("no_tag"), "no_tag");
    EXPECT_EQ(strip_tag_block(""), "");
}

