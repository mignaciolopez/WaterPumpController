#include <gtest/gtest.h>

#include <vector>

#include "adapters/sensors/A02YYUW_Frame.h"

using CE::Adapters::A02YYUW_FrameParser;

namespace
{
    // Feeds all bytes; returns every distance (mm) that was reported.
    std::vector<uint16_t> FeedAll(A02YYUW_FrameParser& parser, const std::vector<uint8_t>& bytes)
    {
        std::vector<uint16_t> results;
        for (const uint8_t b : bytes)
        {
            uint16_t mm = 0;
            if (parser.Feed(b, mm)) results.push_back(mm);
        }
        return results;
    }
}

TEST(A02YYUW_FrameParserTest, ParsesAValidFrameAsMillimetres)
{
    A02YYUW_FrameParser parser;

    // 0x05DC = 1500 mm; checksum = (0xFF + 0x05 + 0xDC) & 0xFF = 0xE0
    const auto results = FeedAll(parser, {0xFF, 0x05, 0xDC, 0xE0});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], 1500);
}

TEST(A02YYUW_FrameParserTest, ParsesSeveralFramesInARow)
{
    A02YYUW_FrameParser parser;

    // 300 mm (0x012C, checksum 0x2C) then 1500 mm
    const auto results = FeedAll(parser, {0xFF, 0x01, 0x2C, 0x2C, 0xFF, 0x05, 0xDC, 0xE0});

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0], 300);
    EXPECT_EQ(results[1], 1500);
}

TEST(A02YYUW_FrameParserTest, SkipsGarbageBeforeTheHeader)
{
    A02YYUW_FrameParser parser;

    const auto results = FeedAll(parser, {0x12, 0x34, 0x56, 0xFF, 0x05, 0xDC, 0xE0});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], 1500);
}

TEST(A02YYUW_FrameParserTest, RejectsABadChecksum)
{
    A02YYUW_FrameParser parser;

    const auto results = FeedAll(parser, {0xFF, 0x05, 0xDC, 0x00});

    EXPECT_TRUE(results.empty());
}

TEST(A02YYUW_FrameParserTest, RecoversWhenAFrameStartsInsideABadOne)
{
    A02YYUW_FrameParser parser;

    // A truncated frame (header + 1 byte) is followed by a complete valid frame.
    const auto results = FeedAll(parser, {0xFF, 0x05, 0xFF, 0x05, 0xDC, 0xE0});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], 1500);
}

TEST(A02YYUW_FrameParserTest, ResetDropsAPartialFrame)
{
    A02YYUW_FrameParser parser;
    FeedAll(parser, {0xFF, 0x05});

    parser.Reset();
    const auto results = FeedAll(parser, {0xFF, 0x01, 0x2C, 0x2C});

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], 300);
}

#include "../support/Main.h"
