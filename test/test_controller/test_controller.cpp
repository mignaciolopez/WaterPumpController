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

    // ---- blind zone (water closer than the sensor's minimum range)

    static App::DistanceSample Silent() { App::DistanceSample s; s.silent = true; s.blindZoneCm = 3.0f; return s; }

    TEST_F(ControllerTest, NoEchoWhileTheHighSensorSeesWaterMeansTheWaterIsInTheBlindZone)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        high.wet = true;
        low.wet = true;

        controller.OnDistance(Silent());
        controller.Update();

        EXPECT_TRUE(controller.HasLevel());
        EXPECT_NEAR(controller.LevelCm(), 137.0f, 0.01f);   // height 140 - minimum distance 3
    }

    TEST_F(ControllerTest, NoEchoWhileTheHighSensorIsDryIsARealFailure)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        controller.OnDistance({true, 90.0f});
        ASSERT_TRUE(controller.HasLevel());
        high.wet = false;

        controller.OnDistance(Silent());

        EXPECT_FALSE(controller.HasLevel());   // unknown, not "full"
    }

    TEST_F(ControllerTest, NoEchoWithoutAKnownBlindZoneIsAFailure)
    {
        high.wet = true;
        App::DistanceSample s;
        s.silent = true;       // blindZoneCm stays 0: the sensor did not say where its blind zone ends

        controller.Execute(SetSetting("medianWindow", 1, 1));
        controller.OnDistance({true, 90.0f});
        controller.OnDistance(s);

        EXPECT_FALSE(controller.HasLevel());
    }

    TEST_F(ControllerTest, AnOutOfRangeReadingIsAFailureEvenWhenTheHighSensorIsWet)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        controller.OnDistance({true, 90.0f});
        high.wet = true;
        low.wet = true;

        controller.OnDistance(App::DistanceSample{});   // not valid, not silent

        EXPECT_FALSE(controller.HasLevel());
    }

    static App::DistanceSample Inferred()
    {
        App::DistanceSample s;
        s.valid = true;
        s.cm = 3.0f;          // the sensor's minimum range
        s.inferred = true;    // it went silent right after reading very close
        return s;
    }

    TEST_F(ControllerTest, AnInferredBlindZoneReadingIsBelievedWhileTheLowSensorSeesWater)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        low.wet = true;
        high.wet = false;   // does not matter for this rule

        controller.OnDistance(Inferred());

        EXPECT_TRUE(controller.HasLevel());
        EXPECT_NEAR(controller.LevelCm(), 137.0f, 0.01f);
    }

    TEST_F(ControllerTest, AnInferredReadingIsNotBelievedWhenTheLowSensorIsDry)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        controller.OnDistance({true, 90.0f});
        ASSERT_TRUE(controller.HasLevel());
        low.wet = false;     // the water cannot be at the top: the sensor has simply stopped answering

        controller.OnDistance(Inferred());

        EXPECT_FALSE(controller.HasLevel());
    }

    TEST_F(ControllerTest, AnInferredReadingIsLoggedOncePerEpisode)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        log.entries.clear();

        for (int i = 0; i < 5; ++i) controller.OnDistance(Inferred());

        int n = 0;
        for (const auto& e : log.entries)
            if (e.message.rfind("No echo right after a reading near the minimum range", 0) == 0) ++n;
        EXPECT_EQ(n, 1);
    }

    TEST_F(ControllerTest, ARealReadingNearTheMinimumNeedsNoCrossCheck)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        low.wet = false;

        controller.OnDistance({true, 4.2f});     // a real echo, not inferred

        EXPECT_TRUE(controller.HasLevel());
        EXPECT_NEAR(controller.LevelCm(), 135.8f, 0.01f);
    }

    TEST_F(ControllerTest, TheBlindZoneAssumptionIsLoggedOncePerEpisode)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        high.wet = true;
        low.wet = true;
        log.entries.clear();

        for (int i = 0; i < 5; ++i) controller.OnDistance(Silent());
        auto count = [&]() {
            int n = 0;
            for (const auto& e : log.entries)
                if (e.message.rfind("No echo while the high sensor sees water", 0) == 0) ++n;
            return n;
        };
        EXPECT_EQ(count(), 1);

        controller.OnDistance({true, 50.0f});   // a real reading ends the episode
        controller.OnDistance(Silent());
        EXPECT_EQ(count(), 2);
    }

    TEST_F(ControllerTest, WaterInTheBlindZoneCountsAsFullForTheLevelRules)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        low.wet = false;
        controller.Update();                    // the pump starts
        clock.Advance(20);
        low.wet = true;
        high.wet = false;
        controller.OnDistance({true, 90.0f});
        controller.Update();
        ASSERT_TRUE(relay.on);

        high.wet = true;                        // the tank filled: the high sensor is wet
        controller.OnDistance(Silent());        // and the radar went silent
        controller.Update();

        EXPECT_FALSE(relay.on);                 // stopped (high sensor), level reads full
        EXPECT_GE(controller.LevelCm(), 130.0f);
    }

    // ---- cooldown log

    static std::vector<std::string> CooldownLogs(const Fakes::FakeLog& log)
    {
        std::vector<std::string> out;
        for (const auto& e : log.entries)
            if (e.message.rfind("Pump on cooldown", 0) == 0) out.push_back(e.message);
        return out;
    }

    // Starts the pump, lets it run its maximum time (20 min) and leaves it on cooldown (60 min) as of "now".
    void RunIntoCooldown(Fakes::FakeFloat& low, Fakes::FakeClock& clock, App::Controller& controller)
    {
        low.wet = false;
        controller.Update();                // the pump starts
        clock.Advance(20 * 60);
        controller.Update();                // max on-time reached: stop + cooldown
    }

    TEST_F(ControllerTest, NothingIsLoggedAboutCooldownWhenThePumpIsNotOnCooldown)
    {
        controller.Update();
        clock.Advance(60);
        controller.Update();

        EXPECT_TRUE(CooldownLogs(log).empty());
    }

    TEST_F(ControllerTest, TheCooldownIsLoggedRightAwayWithTheMinutesLeft)
    {
        RunIntoCooldown(low, clock, controller);

        const auto logs = CooldownLogs(log);
        ASSERT_EQ(logs.size(), 1u);
        EXPECT_EQ(logs[0], "Pump on cooldown: 60 min left");
    }

    TEST_F(ControllerTest, TheCooldownIsLoggedEveryTenSecondsNotMoreOften)
    {
        RunIntoCooldown(low, clock, controller);
        ASSERT_EQ(CooldownLogs(log).size(), 1u);

        clock.Advance(9);
        controller.Update();
        EXPECT_EQ(CooldownLogs(log).size(), 1u);    // 9 s: still quiet

        clock.Advance(1);
        controller.Update();
        EXPECT_EQ(CooldownLogs(log).size(), 2u);    // 10 s: logged again

        controller.Update();
        controller.Update();
        EXPECT_EQ(CooldownLogs(log).size(), 2u);    // same second: no repeat
    }

    TEST_F(ControllerTest, TheMinutesLeftCountDownAndAreRoundedUp)
    {
        RunIntoCooldown(low, clock, controller);

        clock.Advance(10 * 60);
        controller.Update();
        clock.Advance(49 * 60 + 30);                // 59 min 30 s after the start: 30 s left
        controller.Update();

        const auto logs = CooldownLogs(log);
        ASSERT_EQ(logs.size(), 3u);
        EXPECT_EQ(logs[1], "Pump on cooldown: 50 min left");
        EXPECT_EQ(logs[2], "Pump on cooldown: 1 min left");
    }

    TEST_F(ControllerTest, TheCooldownLogStopsWhenTheCooldownEnds)
    {
        RunIntoCooldown(low, clock, controller);

        clock.Advance(60 * 60);                     // the cooldown is over
        controller.Update();
        const size_t before = CooldownLogs(log).size();
        clock.Advance(30);
        controller.Update();

        EXPECT_EQ(CooldownLogs(log).size(), before);
    }

    TEST_F(ControllerTest, ANewCooldownIsAnnouncedRightAway)
    {
        RunIntoCooldown(low, clock, controller);
        clock.Advance(60 * 60);                     // first cooldown over (the pump restarts, the low sensor is dry)
        controller.Update();
        clock.Advance(20 * 60);
        controller.Update();                        // runs its maximum time again: second cooldown

        const auto logs = CooldownLogs(log);
        ASSERT_GE(logs.size(), 2u);
        EXPECT_EQ(logs.back(), "Pump on cooldown: 60 min left");
    }

    // ---- Execute (REST) and Revision

    TEST_F(ControllerTest, ExecuteReturnsTheOutcomeAndSendsNoResponse)
    {
        low.wet = false;

        const auto outcome = controller.Execute(PumpSet(true, 1));

        EXPECT_EQ(outcome.error, ErrorCode::None);
        EXPECT_TRUE(relay.on);
        EXPECT_TRUE(sink.sent.empty());
        EXPECT_FALSE(sink.broadcasts.empty());   // WebSocket clients still hear about the new state
    }

    TEST_F(ControllerTest, ExecuteReportsRefusals)
    {
        high.wet = true;

        EXPECT_EQ(controller.Execute(PumpSet(true, 1)).error, ErrorCode::HighLevel);
        EXPECT_TRUE(sink.sent.empty());
    }

    TEST_F(ControllerTest, ExecuteSettingsBroadcastsTheChange)
    {
        const auto outcome = controller.Execute(SetSetting("pumpMaxTimeOnM", 30, 1));

        EXPECT_TRUE(outcome.settingsChanged);
        EXPECT_EQ(controller.GetSettings().pumpMaxTimeOnM, 30);
        bool announced = false;
        for (const auto& b : sink.broadcasts) announced = announced || Parse(b)["event"] == std::string("settings.changed");
        EXPECT_TRUE(announced);
    }

    TEST_F(ControllerTest, RevisionOnlyChangesWhenSomethingChanged)
    {
        controller.Update();
        const uint32_t start = controller.Revision();

        controller.Update();
        EXPECT_EQ(controller.Revision(), start);

        low.wet = false;           // pump starts
        controller.Update();
        const uint32_t afterPump = controller.Revision();
        EXPECT_GT(afterPump, start);

        Feed(90);                  // level becomes known
        controller.Update();
        const uint32_t afterLevel = controller.Revision();
        EXPECT_GT(afterLevel, afterPump);

        controller.Execute(SetSetting("minLevelCm", 60, 1));
        EXPECT_GT(controller.Revision(), afterLevel);
    }

    TEST_F(ControllerTest, StateAccessorsReflectTheCurrentState)
    {
        Feed(90);
        low.wet = false;
        controller.Update();

        EXPECT_TRUE(controller.GetPumpState().isOn);
        EXPECT_TRUE(controller.HasLevel());
        EXPECT_NEAR(controller.LevelCm(), 50.0f, 0.01f);
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

    TEST_F(ControllerTest, FirstMedianIsLoggedRightAway)
    {
        for (int i = 0; i < 5; ++i) controller.OnDistance({true, 90.0f});

        const auto logs = LogsFor(log, "A02YYUW");
        ASSERT_EQ(logs.size(), 1u);
        EXPECT_EQ(logs[0], "New median: 90.0 cm (water level 50.0 cm) (90 cm)");
    }

    TEST_F(ControllerTest, MedianIsLoggedAtMostOnceEveryTenSeconds)
    {
        for (int i = 0; i < 5; ++i) controller.OnDistance({true, 90.0f});
        ASSERT_EQ(LogsFor(log, "A02YYUW").size(), 1u);

        clock.Advance(9);
        controller.OnDistance({true, 90.0f});
        controller.OnDistance({true, 90.0f});
        EXPECT_EQ(LogsFor(log, "A02YYUW").size(), 1u);    // 9 s: still quiet

        clock.Advance(1);
        controller.OnDistance({true, 90.0f});
        const auto logs = LogsFor(log, "A02YYUW");
        ASSERT_EQ(logs.size(), 2u);                        // 10 s: logged again
        EXPECT_EQ(logs[1], "New median: 90.0 cm (water level 50.0 cm) (90, 90, 90, 90 cm)");
    }

    TEST_F(ControllerTest, MedianListIsWholeNumbersOldestFirstWithCmOnlyOnTheLast)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));   // window of 1: the median is the latest sample
        log.entries.clear();

        controller.OnDistance({true, 80.4f});    // logged right away
        controller.OnDistance({true, 81.6f});
        controller.OnDistance({true, 90.0f});
        clock.Advance(10);
        controller.OnDistance({true, 100.0f});

        const auto logs = LogsFor(log, "A02YYUW");
        ASSERT_EQ(logs.size(), 2u);
        EXPECT_EQ(logs[0], "New median: 80.4 cm (water level 59.6 cm) (80 cm)");
        EXPECT_EQ(logs[1], "New median: 100.0 cm (water level 40.0 cm) (80, 82, 90, 100 cm)");
    }

    TEST_F(ControllerTest, MedianListKeepsOnlyTheLastFifteen)
    {
        controller.Execute(SetSetting("medianWindow", 1, 1));
        log.entries.clear();

        for (int i = 1; i <= 20; ++i) controller.OnDistance({true, static_cast<float>(i)});   // 1..20 (3 cm minimum is irrelevant here)
        clock.Advance(10);
        controller.OnDistance({true, 21.0f});

        const auto logs = LogsFor(log, "A02YYUW");
        ASSERT_EQ(logs.size(), 2u);
        EXPECT_EQ(logs[1], "New median: 21.0 cm (water level 119.0 cm) (7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21 cm)");
    }

    TEST_F(ControllerTest, MedianLogShowsTheFilteredValueNotTheRawSample)
    {
        for (const float cm : {90.0f, 90.0f, 90.0f, 90.0f, 20.0f}) controller.OnDistance({true, cm});

        const auto logs = LogsFor(log, "A02YYUW");
        ASSERT_EQ(logs.size(), 1u);
        EXPECT_EQ(logs[0], "New median: 90.0 cm (water level 50.0 cm) (90 cm)");
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

    TEST_F(ControllerTest, AFailedSaveIsLoggedAndAnswered)
    {
        store.failCommits = true;

        controller.OnCommand(1, SetSetting("pumpMaxTimeOnM", 30, 5));

        EXPECT_EQ(sink.sent[0].message, R"({"id":5,"ok":false,"err":"INTERNAL"})");
        bool logged = false;
        for (const auto& e : log.entries)
            logged = logged || (e.tag == "Controller" && e.message == "Command failed: the settings could not be saved");
        EXPECT_TRUE(logged);
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
