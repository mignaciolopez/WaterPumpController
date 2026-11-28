#include <gtest/gtest.h>

#include "../support/Fakes.h"
#include "core/Tank.h"

using namespace CE;
using namespace CE::Core;

namespace
{
    // Tank height 140 cm: distance 90 -> level 50 (at min), distance 10 -> level 130 (at max).
    class TankTest : public ::testing::Test
    {
    protected:
        Fakes::FakeClock clock;
        Fakes::FakeSwitch relay;
        Fakes::FakeFloat low;
        Fakes::FakeFloat high;
        Fakes::FakeStore store;
        Fakes::FakeLog log;
        Settings settings;
        PumpController pump{relay, low, high, clock, store, log};
        Tank tank{pump, clock, settings};
        LevelRule levelRule;
        NightRule nightRule;

        void SetUp() override
        {
            settings.medianWindow = 3;
            low.wet = true; // float switches say there is enough water; rules decide via the level
            pump.Begin();
            ASSERT_TRUE(tank.AddRule(levelRule));
            ASSERT_TRUE(tank.AddRule(nightRule));
        }

        void Feed(const float distanceCm)
        {
            for (int i = 0; i < 3; ++i) tank.OnDistanceSample(distanceCm);
        }
    };

    TEST_F(TankTest, NoLevelUntilWindowIsFull)
    {
        tank.OnDistanceSample(90);
        tank.OnDistanceSample(90);
        tank.Update();

        EXPECT_FALSE(tank.HasLevel());
        EXPECT_FALSE(relay.on);

        tank.OnDistanceSample(90);
        EXPECT_TRUE(tank.HasLevel());
        EXPECT_FLOAT_EQ(tank.LevelCm(), 50.0f);
    }

    TEST_F(TankTest, StartsPumpWhenLevelIsLowDuringTheDay)
    {
        Feed(95); // level 45

        tank.Update();

        EXPECT_TRUE(relay.on);
    }

    TEST_F(TankTest, DoesNotStartAtNightAboveCriticalLevel)
    {
        clock.localHour = 23;
        Feed(95); // level 45, critical is 20

        tank.Update();

        EXPECT_FALSE(relay.on);
    }

    TEST_F(TankTest, StartsAtNightWhenLevelIsCritical)
    {
        clock.localHour = 23;
        Feed(125); // level 15

        tank.Update();

        EXPECT_TRUE(relay.on);
    }

    TEST_F(TankTest, NightVetoAlsoBlocksFloatSwitchAutoStart)
    {
        clock.localHour = 23;
        Feed(95); // level 45
        low.wet = false; // float switch says "pump", the night rule says no

        tank.Update();

        EXPECT_FALSE(relay.on);
    }

    TEST_F(TankTest, StopsWhenLevelReachesMaxAfterMinRunTime)
    {
        Feed(95);
        tank.Update();
        ASSERT_TRUE(relay.on);

        clock.Advance(30);
        Feed(10); // level 130
        tank.Update();

        EXPECT_FALSE(relay.on);
    }

    TEST_F(TankTest, LevelStopWaitsForMinRunTime)
    {
        Feed(95);
        tank.Update();
        ASSERT_TRUE(relay.on);

        clock.Advance(3);
        Feed(10);
        tank.Update();
        EXPECT_TRUE(relay.on); // too soon, asked again next update

        clock.Advance(10);
        tank.Update();
        EXPECT_FALSE(relay.on);
    }

    TEST_F(TankTest, DistanceFailureMakesLevelUnknownAndStopsLevelDecisions)
    {
        Feed(95);
        ASSERT_TRUE(tank.HasLevel());

        tank.OnDistanceFailure();
        tank.Update();

        EXPECT_FALSE(tank.HasLevel());
        EXPECT_FALSE(relay.on);
    }

    TEST_F(TankTest, FloatSwitchSafetyStillWorksWithoutLevel)
    {
        low.wet = false;
        high.wet = false;
        tank.Update(); // no level, float switch asks for water
        ASSERT_TRUE(relay.on);

        low.wet = true;
        high.wet = true;
        tank.Update();

        EXPECT_FALSE(relay.on);
    }

    TEST_F(TankTest, SettingsChangesApplyOnNextUpdate)
    {
        settings.pumpMaxTimeOnM = 1;
        Feed(95);
        tank.Update();
        ASSERT_TRUE(relay.on);

        clock.Advance(60);
        tank.Update();

        EXPECT_FALSE(relay.on);
        EXPECT_EQ(pump.Mode(), PumpMode::Cooldown);
    }

    TEST_F(TankTest, RuleListHasALimit)
    {
        Tank full(pump, clock, settings);
        for (size_t i = 0; i < Tank::kMaxRules; ++i) ASSERT_TRUE(full.AddRule(levelRule));

        EXPECT_FALSE(full.AddRule(levelRule));
    }
}

#include "../support/Main.h"
