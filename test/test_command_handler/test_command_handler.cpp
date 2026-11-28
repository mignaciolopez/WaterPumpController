#include <gtest/gtest.h>

#include <cstring>

#include "../support/Fakes.h"
#include "core/CommandHandler.h"

using namespace CE;
using namespace CE::Core;

namespace
{
    class CommandHandlerTest : public ::testing::Test
    {
    protected:
        Fakes::FakeClock clock;
        Fakes::FakeSwitch relay;
        Fakes::FakeFloat low;
        Fakes::FakeFloat high;
        Fakes::FakeStore pumpStore;
        Fakes::FakeStore settingsStore;
        Fakes::FakeLog log;
        Settings settings;
        PumpController pump{relay, low, high, clock, pumpStore, log};
        CommandHandler handler{pump, settings, settingsStore};

        void SetUp() override
        {
            low.wet = true;
            pump.Begin();
        }

        static Command SettingsCommand(std::initializer_list<std::pair<const char*, int64_t>> values)
        {
            Command c;
            c.type = CommandType::SettingsSet;
            for (const auto& [name, value] : values)
            {
                std::strcpy(c.values[c.valueCount].name, name);
                c.values[c.valueCount].value = value;
                ++c.valueCount;
            }
            return c;
        }
    };

    TEST_F(CommandHandlerTest, PumpSetOnStartsThePump)
    {
        low.wet = false;
        Command c;
        c.type = CommandType::PumpSet;
        c.on = true;

        const auto outcome = handler.Handle(c);

        EXPECT_EQ(outcome.error, ErrorCode::None);
        EXPECT_TRUE(relay.on);
    }

    TEST_F(CommandHandlerTest, PumpSetOnIsRefusedWhenTankIsFull)
    {
        high.wet = true;
        Command c;
        c.type = CommandType::PumpSet;
        c.on = true;

        EXPECT_EQ(handler.Handle(c).error, ErrorCode::HighLevel);
        EXPECT_FALSE(relay.on);
    }

    TEST_F(CommandHandlerTest, PumpSetOffRightAfterStartIsTooSoon)
    {
        Command on;
        on.type = CommandType::PumpSet;
        on.on = true;
        ASSERT_EQ(handler.Handle(on).error, ErrorCode::None);

        Command off = on;
        off.on = false;

        EXPECT_EQ(handler.Handle(off).error, ErrorCode::PumpTooSoon);
        EXPECT_TRUE(relay.on);
    }

    TEST_F(CommandHandlerTest, GetCommandsChangeNothing)
    {
        Command c;
        c.type = CommandType::StateGet;
        EXPECT_EQ(handler.Handle(c).error, ErrorCode::None);
        c.type = CommandType::SettingsGet;
        const auto outcome = handler.Handle(c);

        EXPECT_EQ(outcome.error, ErrorCode::None);
        EXPECT_FALSE(outcome.settingsChanged);
        EXPECT_FALSE(relay.on);
    }

    TEST_F(CommandHandlerTest, SettingsSetAppliesAndPersists)
    {
        const auto outcome = handler.Handle(SettingsCommand({{"minLevelCm", 90}, {"maxLevelCm", 100}}));

        EXPECT_EQ(outcome.error, ErrorCode::None);
        EXPECT_TRUE(outcome.settingsChanged);
        EXPECT_EQ(settings.minLevelCm, 90);
        EXPECT_EQ(settingsStore.values["set.minLevelCm"], 90);
    }

    TEST_F(CommandHandlerTest, InvalidSettingsAreRejectedAndNothingChanges)
    {
        const auto outcome = handler.Handle(SettingsCommand({{"pumpMaxTimeOnM", 30}, {"medianWindow", 99}}));

        EXPECT_EQ(outcome.error, ErrorCode::InvalidValue);
        EXPECT_FALSE(outcome.settingsChanged);
        EXPECT_EQ(settings.pumpMaxTimeOnM, 20);
        EXPECT_TRUE(settingsStore.values.empty());
    }

    TEST_F(CommandHandlerTest, UnknownSettingIsReported)
    {
        EXPECT_EQ(handler.Handle(SettingsCommand({{"nope", 1}})).error, ErrorCode::UnknownField);
    }

    TEST_F(CommandHandlerTest, InconsistentSettingsAreInvalidValue)
    {
        EXPECT_EQ(handler.Handle(SettingsCommand({{"minLevelCm", 130}})).error, ErrorCode::InvalidValue);
    }

    TEST_F(CommandHandlerTest, SettingsAreRolledBackWhenTheCommitFails)
    {
        settingsStore.failCommits = true;

        const auto outcome = handler.Handle(SettingsCommand({{"pumpMaxTimeOnM", 30}}));

        EXPECT_EQ(outcome.error, ErrorCode::Internal);
        EXPECT_EQ(settings.pumpMaxTimeOnM, 20);
    }

    TEST_F(CommandHandlerTest, SettingsAreRolledBackWhenTheyCannotBeSaved)
    {
        settingsStore.failWrites = true;

        const auto outcome = handler.Handle(SettingsCommand({{"pumpMaxTimeOnM", 30}}));

        EXPECT_EQ(outcome.error, ErrorCode::Internal);
        EXPECT_FALSE(outcome.settingsChanged);
        EXPECT_EQ(settings.pumpMaxTimeOnM, 20);
    }
}

#include "../support/Main.h"
