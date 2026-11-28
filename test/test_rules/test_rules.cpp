#include <gtest/gtest.h>

#include "core/rules/Rules.h"

using namespace CE::Core;

namespace
{
    // Defaults: critical 20, min 50, max 130, night 22 -> 8.
    RuleContext MakeContext(const Settings& s, const bool hasLevel, const float level, const bool pumpOn = false,
                            const bool timeValid = true, const int hour = 12)
    {
        return RuleContext{s, hasLevel, level, pumpOn, timeValid, hour};
    }
}

TEST(IsNightHourTest, WindowSpanningMidnight)
{
    EXPECT_TRUE(IsNightHour(22, 22, 8));
    EXPECT_TRUE(IsNightHour(23, 22, 8));
    EXPECT_TRUE(IsNightHour(0, 22, 8));
    EXPECT_TRUE(IsNightHour(7, 22, 8));
    EXPECT_FALSE(IsNightHour(8, 22, 8));
    EXPECT_FALSE(IsNightHour(12, 22, 8));
    EXPECT_FALSE(IsNightHour(21, 22, 8));
}

TEST(IsNightHourTest, WindowWithinOneDay)
{
    EXPECT_TRUE(IsNightHour(1, 1, 8));
    EXPECT_TRUE(IsNightHour(7, 1, 8));
    EXPECT_FALSE(IsNightHour(8, 1, 8));
    EXPECT_FALSE(IsNightHour(0, 1, 8));
}

TEST(IsNightHourTest, EmptyWindowIsNeverNight)
{
    for (int h = 0; h < 24; ++h) EXPECT_FALSE(IsNightHour(h, 5, 5));
}

TEST(LevelRuleTest, WantsOnAtOrBelowMinLevelWhenOff)
{
    const Settings s;
    const LevelRule rule;

    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 50)), RuleAction::WantOn);
    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 10)), RuleAction::WantOn);
    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 51)), RuleAction::NoOpinion);
}

TEST(LevelRuleTest, WantsOffAtOrAboveMaxLevelWhenOn)
{
    const Settings s;
    const LevelRule rule;

    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 130, true)), RuleAction::WantOff);
    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 129, true)), RuleAction::NoOpinion);
}

TEST(LevelRuleTest, KeepsRunningBetweenMinAndMax)
{
    const Settings s;
    const LevelRule rule;

    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 40, true)), RuleAction::NoOpinion);
    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 100, false)), RuleAction::NoOpinion);
}

TEST(LevelRuleTest, NoOpinionWithoutLevel)
{
    const Settings s;
    const LevelRule rule;

    EXPECT_EQ(rule.Evaluate(MakeContext(s, false, 0)), RuleAction::NoOpinion);
}

TEST(NightRuleTest, VetoesAtNightAboveCriticalLevel)
{
    const Settings s;
    const NightRule rule;

    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 40, false, true, 23)), RuleAction::VetoOn);
    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 21, false, true, 3)), RuleAction::VetoOn);
}

TEST(NightRuleTest, AllowsAtNightAtOrBelowCriticalLevel)
{
    const Settings s;
    const NightRule rule;

    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 20, false, true, 23)), RuleAction::NoOpinion);
    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 5, false, true, 2)), RuleAction::NoOpinion);
}

TEST(NightRuleTest, AllowsDuringTheDay)
{
    const Settings s;
    const NightRule rule;

    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 40, false, true, 12)), RuleAction::NoOpinion);
}

TEST(NightRuleTest, NoOpinionWithoutLevelOrValidTime)
{
    const Settings s;
    const NightRule rule;

    EXPECT_EQ(rule.Evaluate(MakeContext(s, false, 0, false, true, 23)), RuleAction::NoOpinion);
    EXPECT_EQ(rule.Evaluate(MakeContext(s, true, 40, false, false, 23)), RuleAction::NoOpinion);
}

#include "../support/Main.h"
