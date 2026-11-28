#include <gtest/gtest.h>

#include "../support/Fakes.h"
#include "core/PumpController.h"

using namespace CE;
using namespace CE::Core;

namespace
{
    // Level sensors: "wet" means the water has reached that sensor.
    // Normal operation: low wet = enough water, low dry = needs pumping, high wet = tank full.
    class PumpControllerTest : public ::testing::Test
    {
    protected:
        Fakes::FakeClock clock;
        Fakes::FakeSwitch relay;
        Fakes::FakeFloat low;
        Fakes::FakeFloat high;
        Fakes::FakeStore store;
        Fakes::FakeLog log;
        PumpConfig config{10, 20 * 60, 60 * 60};
        PumpController pump{relay, low, high, clock, store, log, config};

        void SetUp() override
        {
            low.wet = true; // tank is at a comfortable level
            pump.Begin();
        }

        void NeedWater() { low.wet = false; high.wet = false; }
    };

    TEST_F(PumpControllerTest, BeginForcesRelayOffAndStartsIdle)
    {
        EXPECT_FALSE(relay.on);
        EXPECT_GE(relay.setCount, 1);
        EXPECT_EQ(pump.Mode(), PumpMode::Idle);
    }

    TEST_F(PumpControllerTest, StaysIdleWhenLevelIsFine)
    {
        pump.Update();

        EXPECT_FALSE(relay.on);
        EXPECT_EQ(pump.Mode(), PumpMode::Idle);
    }

    TEST_F(PumpControllerTest, StartsWhenLowSensorIsDry)
    {
        NeedWater();

        pump.Update();

        EXPECT_TRUE(relay.on);
        EXPECT_EQ(pump.Mode(), PumpMode::Running);
        EXPECT_EQ(pump.State().timeStampOn, clock.now);
    }

    TEST_F(PumpControllerTest, StopsImmediatelyWhenHighSensorIsWet)
    {
        NeedWater();
        pump.Update();
        clock.Advance(1); // well inside the min restart interval
        low.wet = true;
        high.wet = true;

        pump.Update();

        EXPECT_FALSE(relay.on);
        EXPECT_EQ(pump.Mode(), PumpMode::Idle);
    }

    TEST_F(PumpControllerTest, ManualOffIsAlwaysAllowed)
    {
        NeedWater();
        pump.Update();

        EXPECT_EQ(pump.Request(false), PumpResult::Ok);
        EXPECT_FALSE(relay.on);
    }

    TEST_F(PumpControllerTest, RefusesRestartWithinMinInterval)
    {
        NeedWater();
        pump.Update();
        pump.Request(false);
        clock.Advance(5);

        EXPECT_EQ(pump.Request(true), PumpResult::TooSoon);
        pump.Update();
        EXPECT_FALSE(relay.on);

        clock.Advance(5);
        pump.Update();
        EXPECT_TRUE(relay.on);
    }

    TEST_F(PumpControllerTest, ManualStartRefusedWhenHighSensorWet)
    {
        low.wet = true;
        high.wet = true;

        EXPECT_EQ(pump.Request(true), PumpResult::HighLevel);
        EXPECT_FALSE(relay.on);
    }

    TEST_F(PumpControllerTest, RequestOnWhileRunningIsOk)
    {
        NeedWater();
        pump.Update();

        EXPECT_EQ(pump.Request(true), PumpResult::Ok);
        EXPECT_TRUE(relay.on);
    }

    TEST_F(PumpControllerTest, MaxOnTimeStopsPumpAndStartsCooldown)
    {
        NeedWater();
        pump.Update();
        clock.Advance(20 * 60);

        pump.Update();

        EXPECT_FALSE(relay.on);
        EXPECT_EQ(pump.Mode(), PumpMode::Cooldown);
        EXPECT_EQ(pump.Request(true), PumpResult::Cooldown);
        pump.Update();
        EXPECT_FALSE(relay.on); // still low on water, but locked out
    }

    TEST_F(PumpControllerTest, CooldownExpiresAndPumpRestarts)
    {
        NeedWater();
        pump.Update();
        clock.Advance(20 * 60);
        pump.Update();

        clock.Advance(60 * 60 - 1);
        pump.Update();
        EXPECT_EQ(pump.Mode(), PumpMode::Cooldown);

        clock.Advance(1);
        pump.Update();
        EXPECT_TRUE(relay.on);
        EXPECT_EQ(pump.Mode(), PumpMode::Running);
    }

    TEST_F(PumpControllerTest, InconsistentSensorsFaultAndStopPump)
    {
        NeedWater();
        pump.Update();
        ASSERT_TRUE(relay.on);

        low.wet = false;
        high.wet = true; // physically impossible
        pump.Update();

        EXPECT_FALSE(relay.on);
        EXPECT_EQ(pump.Mode(), PumpMode::Fault);
        EXPECT_EQ(pump.State().fault, PumpFault::SensorInconsistent);
        EXPECT_EQ(pump.Request(true), PumpResult::Fault);
    }

    TEST_F(PumpControllerTest, FaultClearsWhenSensorsAreConsistentAgain)
    {
        low.wet = false;
        high.wet = true;
        pump.Update();
        ASSERT_EQ(pump.Mode(), PumpMode::Fault);

        high.wet = false;
        pump.Update();

        EXPECT_EQ(pump.State().fault, PumpFault::None);
    }

    TEST_F(PumpControllerTest, UnsyncedClockFaultsAndStopsPump)
    {
        NeedWater();
        pump.Update();
        ASSERT_TRUE(relay.on);

        clock.synced = false;
        pump.Update();

        EXPECT_FALSE(relay.on);
        EXPECT_EQ(pump.State().fault, PumpFault::NoTime);

        clock.synced = true;
        pump.Update();
        EXPECT_EQ(pump.State().fault, PumpFault::None);
    }

    TEST_F(PumpControllerTest, FaultDoesNotEraseCooldown)
    {
        NeedWater();
        pump.Update();
        clock.Advance(20 * 60);
        pump.Update();
        ASSERT_EQ(pump.Mode(), PumpMode::Cooldown);

        clock.synced = false;
        pump.Update();
        clock.synced = true;
        pump.Update();

        EXPECT_TRUE(pump.State().isOnCooldown);
        EXPECT_FALSE(relay.on);
    }

    TEST_F(PumpControllerTest, CooldownSurvivesReboot)
    {
        NeedWater();
        pump.Update();
        clock.Advance(20 * 60);
        pump.Update();
        ASSERT_EQ(pump.Mode(), PumpMode::Cooldown);

        Fakes::FakeSwitch relay2;
        PumpController rebooted(relay2, low, high, clock, store, log, config);
        rebooted.Begin();

        EXPECT_EQ(rebooted.Mode(), PumpMode::Cooldown);
        rebooted.Update();
        EXPECT_FALSE(relay2.on);
    }

    TEST_F(PumpControllerTest, RunningPumpIsOffAfterRebootAndRespectsMinInterval)
    {
        NeedWater();
        pump.Update();
        ASSERT_TRUE(relay.on);

        clock.Advance(3);
        Fakes::FakeSwitch relay2;
        PumpController rebooted(relay2, low, high, clock, store, log, config);
        rebooted.Begin();

        EXPECT_FALSE(relay2.on);
        EXPECT_EQ(rebooted.Mode(), PumpMode::Idle);
        EXPECT_EQ(rebooted.Request(true), PumpResult::TooSoon);
    }

    TEST_F(PumpControllerTest, MissingStoreUsesDefaultsAndWritesThem)
    {
        Fakes::FakeStore empty;
        Fakes::FakeSwitch relay2;
        PumpController fresh(relay2, low, high, clock, empty, log, config);

        fresh.Begin();

        EXPECT_EQ(fresh.Mode(), PumpMode::Idle);
        EXPECT_FALSE(empty.values.empty());
    }

    TEST_F(PumpControllerTest, StoreWriteFailureIsLoggedButPumpStillWorks)
    {
        store.failWrites = true;
        NeedWater();

        pump.Update();

        EXPECT_TRUE(relay.on);
        bool logged = false;
        for (const auto& m : log.messages) logged = logged || m == "Failed to persist state";
        EXPECT_TRUE(logged);
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
