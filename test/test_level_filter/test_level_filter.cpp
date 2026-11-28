#include <gtest/gtest.h>

#include "core/LevelFilter.h"

using CE::Core::LevelFilter;

TEST(LevelFilterTest, NotReadyUntilWindowIsFull)
{
    LevelFilter filter(3);

    filter.Add(10);
    filter.Add(20);

    EXPECT_FALSE(filter.Ready());
    EXPECT_FLOAT_EQ(filter.Median(), 0.0f);

    filter.Add(30);
    EXPECT_TRUE(filter.Ready());
}

TEST(LevelFilterTest, OddWindowReturnsMiddleValue)
{
    LevelFilter filter(5);
    for (const float v : {100.0f, 3.0f, 50.0f, 52.0f, 51.0f}) filter.Add(v);

    EXPECT_FLOAT_EQ(filter.Median(), 51.0f); // sorted: 3, 50, 51, 52, 100
}

TEST(LevelFilterTest, EvenWindowAveragesTwoMiddleValues)
{
    LevelFilter filter(4);
    for (const float v : {10.0f, 40.0f, 20.0f, 30.0f}) filter.Add(v);

    EXPECT_FLOAT_EQ(filter.Median(), 25.0f);
}

TEST(LevelFilterTest, WindowSlidesAndDropsOldestSample)
{
    LevelFilter filter(3);
    for (const float v : {1.0f, 2.0f, 3.0f}) filter.Add(v);
    ASSERT_FLOAT_EQ(filter.Median(), 2.0f);

    filter.Add(100.0f); // window is now 2, 3, 100

    EXPECT_FLOAT_EQ(filter.Median(), 3.0f);
}

TEST(LevelFilterTest, ResetClearsSamples)
{
    LevelFilter filter(2);
    filter.Add(1);
    filter.Add(2);

    filter.Reset();

    EXPECT_FALSE(filter.Ready());
}

TEST(LevelFilterTest, ChangingWindowResetsButSameWindowKeepsSamples)
{
    LevelFilter filter(2);
    filter.Add(1);
    filter.Add(2);

    filter.SetWindow(2);
    EXPECT_TRUE(filter.Ready());

    filter.SetWindow(3);
    EXPECT_FALSE(filter.Ready());
}

TEST(LevelFilterTest, WindowIsClamped)
{
    LevelFilter filter(0);
    filter.Add(7);
    EXPECT_TRUE(filter.Ready()); // clamped to 1

    filter.SetWindow(1000);
    for (size_t i = 0; i < LevelFilter::kMaxWindow; ++i) filter.Add(static_cast<float>(i));
    EXPECT_TRUE(filter.Ready()); // clamped to kMaxWindow
}

#include "../support/Main.h"
