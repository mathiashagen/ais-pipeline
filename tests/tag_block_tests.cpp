#include "ais/tag_block.hpp"
#include <gtest/gtest.h>

TEST(TagBlockTests, StripTagBlock) {
    using ais::strip_tag_block;

    EXPECT_EQ(strip_tag_block("\\tag\\content"), std::optional<std::string_view>("content"));
    EXPECT_EQ(strip_tag_block("\\tag\\"), std::optional<std::string_view>(""));
    EXPECT_EQ(strip_tag_block("no_tag"), "no_tag");
    EXPECT_EQ(strip_tag_block(""), "");
}

TEST(TagBlockTests, TagBlockSource) {
    using ais::tag_block_source;

    EXPECT_EQ(tag_block_source("\\s:2573505,c:1787171836*0C\\!BSVDM,…"), std::optional<std::string_view>("2573505"));
    EXPECT_EQ(tag_block_source("\\c:1787171836,s:2573505*0C\\!BSVDM,…"), std::optional<std::string_view>("2573505"));
    EXPECT_EQ(tag_block_source("\\c:1787171836*0C\\…"), std::nullopt);
    EXPECT_EQ(tag_block_source("!AIVDM,…"), std::nullopt);
    EXPECT_EQ(tag_block_source("\\s:12345"), std::nullopt);
}
