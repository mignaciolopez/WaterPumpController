#include <gtest/gtest.h>

#include <ArduinoJson.h>

#include <cstring>

#include "../support/Fakes.h"
#include "app/Controller.h"

using namespace CE;
using namespace CE::Core;

namespace
{
    JsonDocument Parse(const std::string& json)
    {
        JsonDocument doc;
        EXPECT_FALSE(deserializeJson(doc, json)) << json;
        return doc;
    }

    class ControllerTest : public ::testing::Test
    {
    protected:
        Fakes::FakeClock clock;
        Fakes::FakeSwitch relay;
        Fakes::FakeFloat low;
        Fakes::FakeFloat high;
        Fakes::FakeStore store;
        Fakes::FakeLog log;
        Fakes::FakeSink sink;
        App::Controller controller{relay, low, high, clock, store, log, sink};

        void SetUp() override
        {
            low.wet = true;
            controller.Begin();
        }

        // Fills the median window (default 5 samples) with the same reading.
        void Feed(const float distanceCm)
        {
            for (int i = 0; i < 5; ++i) controller.OnDistance({true, distanceCm});
        }

        static Command Cmd(const CommandType type, const int32_t id)
        {
            Command c;
            c.type = type;
            c.id = id;
            return c;
        }

        static Command PumpSet(const bool on, const int32_t id)
        {
            Command c = Cmd(CommandType::PumpSet, id);
            c.on = on;
            return c;
        }

        static Command SetSetting(const char* name, const int64_t value, const int32_t id)
        {
            Command c = Cmd(CommandType::SettingsSet, id);
            std::strcpy(c.values[0].name, name);
            c.values[0].value = value;
            c.valueCount = 1;
            return c;
        }
    };

    TEST_F(ControllerTest, FirstUpdatePublishesPumpStateAndLevel)
    {
        controller.Update();

        ASSERT_EQ(sink.broadcasts.size(), 2u);
        EXPECT_STREQ(Parse(sink.broadcasts[0])["event"], "pump.state");
        EXPECT_STREQ(Parse(sink.broadcasts[1])["event"], "tank.level");
        EXPECT_TRUE(Parse(sink.broadcasts[1])["data"]["cm"].isNull());
    }

    // ---- logging

    std::vector<std::string> LogsFor(const Fakes::FakeLog& log, const std::string& tag)
    {
        std::vector<std::string> out;
        for (const auto& e : log.entries)
            if (e.tag == tag) out.push_back(e.message);
        return out;
    }

    TEST_F(ControllerTest, FirstUpdateLogsTheStateOfBothXkcSensors)
    {
        controller.Update();

        const auto logs = LogsFor(log, "XKC_Y23");
        ASSERT_EQ(logs.size(), 2u);
        EXPECT_EQ(logs[0], "Low: water detected");
        EXPECT_EQ(logs[1], "High: no water");
    }

    TEST_F(ControllerTest, XkcChangesAreLoggedOnceEach)
    {
        controller.Update();
        log.entries.clear();

        controller.Update();                   // nothing changed
        EXPECT_TRUE(LogsFor(log, "XKC_Y23").empty());

        low.wet = false;
        controller.Update();
        controller.Update();                   // still the same state
        auto logs = LogsFor(log, "XKC_Y23");
        ASSERT_EQ(logs.size(), 1u);
        EXPECT_EQ(logs[0], "Low: no water");

        log.entries.clear();
        low.wet = true;
        high.wet = true;
        controller.Update();
        logs = LogsFor(log, "XKC_Y23");
        ASSERT_EQ(logs.size(), 2u);
        EXPECT_EQ(logs[0], "Low: water detected");
        EXPECT_EQ(logs[1], "High: water detected");
    }

    TEST_F(ControllerTest, NoMedianLogUntilTheWindowIsFull)
    {
        for (int i = 0; i < 4; ++i) controller.OnDistance({true, 90.0f});

        EXPECT_TRUE(LogsFor(log, "A02YYUW").empty());
    }

    TEST_F(ControllerTest, EveryNewMedianIsLogged)
    {
        for (int i = 0; i < 5; ++i) controller.OnDistance({true, 90.0f});
        auto logs = LogsFor(log, "A02YYUW");
        ASSERT_EQ(logs.size(), 1u);
        EXPECT_EQ(logs[0], "New median: 90.0 cm (water level 50.0 cm)");

        controller.OnDistance({true, 90.0f});
        controller.OnDistance({true, 90.0f});
        EXPECT_EQ(LogsFor(log, "A02YYUW").size(), 3u);
    }

    TEST_F(ControllerTest, MedianLogShowsTheFilteredValueNotTheRawSample)
    {
        for (const float cm : {90.0f, 90.0f, 90.0f, 90.0f, 20.0f}) controller.OnDistance({true, cm});

        const auto logs = LogsFor(log, "A02YYUW");
        ASSERT_EQ(logs.size(), 1u);
        EXPECT_EQ(logs[0], "New median: 90.0 cm (water level 50.0 cm)");
    }

    TEST_F(ControllerTest, FailedReadingsAreNotLoggedAsMedians)
    {
        for (int i = 0; i < 5; ++i) controller.OnDistance({true, 90.0f});
        log.entries.clear();

        controller.OnDistance({false, 0.0f});

        EXPECT_TRUE(LogsFor(log, "A02YYUW").empty());
    }

    TEST_F(ControllerTest, NothingIsRepublishedWhenNothingChanged)
    {
        controller.Update();
        sink.broadcasts.clear();

        controller.Update();
        controller.Update();

        EXPECT_TRUE(sink.broadcasts.empty());
    }

    TEST_F(ControllerTest, LevelEventIsPublishedWhenTheLevelChanges)
    {
        controller.Update();
        sink.broadcasts.clear();

        Feed(90); // tank height 140 -> level 50
        controller.Update();

        ASSERT_EQ(sink.broadcasts.size(), 1u);
        const auto doc = Parse(sink.broadcasts[0]);
        EXPECT_STREQ(doc["event"], "tank.level");
        EXPECT_NEAR(doc["data"]["cm"].as<float>(), 50.0f, 0.01f);
    }

    TEST_F(ControllerTest, FloatSwitchStartsPumpAndTheChangeIsBroadcast)
    {
        controller.Update();
        sink.broadcasts.clear();

        low.wet = false;
        controller.Update();

        EXPECT_TRUE(relay.on);
        ASSERT_EQ(sink.broadcasts.size(), 1u);
        const auto doc = Parse(sink.broadcasts[0]);
        EXPECT_STREQ(doc["event"], "pump.state");
        EXPECT_STREQ(doc["data"]["mode"], "running");
    }

    TEST_F(ControllerTest, FailedDistanceReadingMakesLevelUnknown)
    {
        Feed(90);
        controller.Update();
        sink.broadcasts.clear();

        controller.OnDistance({false, 0.0f});
        controller.Update();

        ASSERT_EQ(sink.broadcasts.size(), 1u);
        EXPECT_TRUE(Parse(sink.broadcasts[0])["data"]["cm"].isNull());
    }

    TEST_F(ControllerTest, StateGetAnswersOnlyTheAskingClient)
    {
        controller.OnCommand(42, Cmd(CommandType::StateGet, 5));

        ASSERT_EQ(sink.sent.size(), 1u);
        EXPECT_EQ(sink.sent[0].clientId, 42u);
        const auto doc = Parse(sink.sent[0].message);
        EXPECT_EQ(doc["id"].as<int>(), 5);
        EXPECT_TRUE(doc["ok"].as<bool>());
        EXPECT_STREQ(doc["data"]["pump"]["mode"], "idle");
    }

    TEST_F(ControllerTest, PumpSetOkAnswersAndPublishesTheNewState)
    {
        low.wet = false;

        controller.OnCommand(7, PumpSet(true, 1));

        EXPECT_TRUE(relay.on);
        ASSERT_EQ(sink.sent.size(), 1u);
        EXPECT_EQ(sink.sent[0].message, R"({"id":1,"ok":true})");
        bool announced = false;
        for (const auto& b : sink.broadcasts)
        {
            const auto doc = Parse(b);
            if (doc["event"] == std::string("pump.state")) announced = std::string(doc["data"]["mode"] | "") == "running";
        }
        EXPECT_TRUE(announced);
    }

    TEST_F(ControllerTest, RefusedPumpCommandReturnsTheErrorCode)
    {
        high.wet = true;

        controller.OnCommand(7, PumpSet(true, 2));

        EXPECT_FALSE(relay.on);
        ASSERT_EQ(sink.sent.size(), 1u);
        EXPECT_EQ(sink.sent[0].message, R"({"id":2,"ok":false,"err":"HIGH_LEVEL"})");
    }

    TEST_F(ControllerTest, SettingsGetReturnsTheSettings)
    {
        controller.OnCommand(1, Cmd(CommandType::SettingsGet, 3));

        ASSERT_EQ(sink.sent.size(), 1u);
        EXPECT_EQ(Parse(sink.sent[0].message)["data"]["heightCm"].as<int>(), 140);
    }

    TEST_F(ControllerTest, SettingsSetAppliesSavesAndBroadcasts)
    {
        controller.OnCommand(1, SetSetting("pumpMaxTimeOnM", 30, 4));

        EXPECT_EQ(controller.GetSettings().pumpMaxTimeOnM, 30);
        EXPECT_EQ(store.values["set.pumpMaxTimeOnM"], 30);
        EXPECT_EQ(sink.sent[0].message, R"({"id":4,"ok":true})");
        bool announced = false;
        for (const auto& b : sink.broadcasts) announced = announced || Parse(b)["event"] == std::string("settings.changed");
        EXPECT_TRUE(announced);
    }

    TEST_F(ControllerTest, InvalidSettingIsRefusedWithoutBroadcast)
    {
        controller.OnCommand(1, SetSetting("medianWindow", 99, 6));

        EXPECT_EQ(sink.sent[0].message, R"({"id":6,"ok":false,"err":"INVALID_VALUE"})");
        EXPECT_EQ(controller.GetSettings().medianWindow, 5);
        for (const auto& b : sink.broadcasts) EXPECT_STRNE(Parse(b)["event"], "settings.changed");
    }

    TEST_F(ControllerTest, NegativeSettingValuesWork)
    {
        controller.OnCommand(1, SetSetting("utcOffsetMinutes", -300, 8));

        EXPECT_EQ(controller.GetSettings().utcOffsetMinutes, -300);
        EXPECT_EQ(sink.sent[0].message, R"({"id":8,"ok":true})");
    }

    TEST_F(ControllerTest, SettingsSurviveARestart)
    {
        controller.OnCommand(1, SetSetting("minLevelCm", 60, 9));

        Fakes::FakeSwitch relay2;
        Fakes::FakeSink sink2;
        App::Controller rebooted(relay2, low, high, clock, store, log, sink2);
        rebooted.Begin();

        EXPECT_EQ(rebooted.GetSettings().minLevelCm, 60);
    }

    TEST_F(ControllerTest, NightModeVetoesFloatSwitchStartAtNight)
    {
        clock.localHour = 23;
        Feed(90); // level 50, critical is 20
        controller.Update();

        low.wet = false;
        controller.Update();

        EXPECT_FALSE(relay.on);
    }

    TEST_F(ControllerTest, ChangedSettingsTakeEffectOnTheNextUpdate)
    {
        controller.OnCommand(1, SetSetting("pumpMaxTimeOnM", 1, 10));
        low.wet = false;
        controller.Update();
        ASSERT_TRUE(relay.on);

        clock.Advance(60);
        controller.Update();

        EXPECT_FALSE(relay.on);
    }
}

#include "../support/Main.h"
