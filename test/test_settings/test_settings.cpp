#include <gtest/gtest.h>

#include "../support/Fakes.h"
#include "core/Messages.h"
#include "core/Settings.h"

using namespace CE;
using namespace CE::Core;

TEST(SettingsTest, DefaultsAreConsistent)
{
    EXPECT_TRUE(IsConsistent(Settings{}));
}

TEST(SettingsTest, EveryFieldInTheTableIsFindableAndWithinItsOwnRange)
{
    const Settings defaults;
    for (size_t i = 0; i < SettingFieldCount(); ++i)
    {
        const auto& field = SettingFieldAt(i);
        EXPECT_EQ(FindSettingField(field.name), &field);
        EXPECT_GE(defaults.*(field.member), field.min) << field.name;
        EXPECT_LE(defaults.*(field.member), field.max) << field.name;
    }
}

TEST(SettingsTest, AllSettingsFitInOneCommand)
{
    // A client may read all settings and send them back in one settings.set.
    EXPECT_LE(SettingFieldCount(), Command::kMaxSettingValues);
}

TEST(SettingsTest, SetAndGetAField)
{
    Settings s;

    EXPECT_EQ(SetSetting(s, "pumpMaxTimeOnM", 30), SettingResult::Ok);

    int64_t value = 0;
    EXPECT_TRUE(GetSetting(s, "pumpMaxTimeOnM", value));
    EXPECT_EQ(value, 30);
}

TEST(SettingsTest, NegativeValuesAreAllowedWhereTheRangeAllowsThem)
{
    Settings s;

    EXPECT_EQ(SetSetting(s, "utcOffsetMinutes", -300), SettingResult::Ok);
    EXPECT_EQ(s.utcOffsetMinutes, -300);
    EXPECT_EQ(SetSetting(s, "utcOffsetMinutes", -721), SettingResult::OutOfRange);
    EXPECT_EQ(SetSetting(s, "utcOffsetMinutes", 841), SettingResult::OutOfRange);
    EXPECT_EQ(SetSetting(s, "medianWindow", -1), SettingResult::OutOfRange);
}

TEST(SettingsTest, NegativeValuesRoundTripThroughTheStore)
{
    Fakes::FakeStore store;
    Settings saved;
    ASSERT_EQ(SetSetting(saved, "utcOffsetMinutes", -240), SettingResult::Ok);
    ASSERT_TRUE(SaveSettings(store, saved));

    Settings loaded;
    LoadSettings(store, loaded);

    EXPECT_EQ(loaded.utcOffsetMinutes, -240);
}

TEST(SettingsTest, UnknownFieldIsRejected)
{
    Settings s;

    EXPECT_EQ(SetSetting(s, "nope", 1), SettingResult::UnknownField);
    int64_t value = 0;
    EXPECT_FALSE(GetSetting(s, "nope", value));
}

TEST(SettingsTest, OutOfRangeIsRejectedAndSettingsUnchanged)
{
    Settings s;

    EXPECT_EQ(SetSetting(s, "medianWindow", 16), SettingResult::OutOfRange);
    EXPECT_EQ(SetSetting(s, "medianWindow", 0), SettingResult::OutOfRange);
    EXPECT_EQ(SetSetting(s, "medianWindow", -1), SettingResult::OutOfRange);
    EXPECT_EQ(s.medianWindow, 5);
}

TEST(SettingsTest, InconsistentLevelsAreRejected)
{
    Settings s;

    EXPECT_EQ(SetSetting(s, "minLevelCm", 130), SettingResult::Inconsistent); // min == max
    EXPECT_EQ(SetSetting(s, "maxLevelCm", 141), SettingResult::Inconsistent); // max > height
    EXPECT_EQ(SetSetting(s, "criticalLevelCm", 50), SettingResult::Inconsistent); // critical == min
    EXPECT_EQ(s.minLevelCm, 50);
}

TEST(SettingsTest, BatchIsAtomicAndAllowsMovingRelatedFieldsTogether)
{
    Settings s;
    const SettingUpdate good[] = {{"minLevelCm", 90}, {"maxLevelCm", 100}, {"criticalLevelCm", 80}};
    EXPECT_EQ(SetSettings(s, good, 3), SettingResult::Ok);
    EXPECT_EQ(s.minLevelCm, 90);

    const SettingUpdate bad[] = {{"pumpMaxTimeOnM", 5}, {"medianWindow", 99}};
    EXPECT_EQ(SetSettings(s, bad, 2), SettingResult::OutOfRange);
    EXPECT_EQ(s.pumpMaxTimeOnM, 20); // first update of the failed batch was not applied
}

TEST(SettingsTest, SaveThenLoadRoundTrips)
{
    Fakes::FakeStore store;
    Settings saved;
    ASSERT_EQ(SetSetting(saved, "pumpCooldownTimeM", 45), SettingResult::Ok);
    ASSERT_TRUE(SaveSettings(store, saved));

    Settings loaded;
    LoadSettings(store, loaded);

    EXPECT_EQ(loaded.pumpCooldownTimeM, 45);
}

TEST(SettingsTest, LoadKeepsDefaultsForMissingAndInvalidValues)
{
    Fakes::FakeStore store;
    store.values["set.medianWindow"] = 999; // out of range
    store.values["set.pumpMaxTimeOnM"] = 30;

    Settings s;
    LoadSettings(store, s);

    EXPECT_EQ(s.medianWindow, 5);
    EXPECT_EQ(s.pumpMaxTimeOnM, 30);
    EXPECT_EQ(s.heightCm, 140);
}

TEST(SettingsTest, LoadIgnoresStoredDataThatIsInconsistent)
{
    Fakes::FakeStore store;
    store.values["set.minLevelCm"] = 200;

    Settings s;
    LoadSettings(store, s);

    EXPECT_EQ(s.minLevelCm, 50);
}

TEST(SettingsTest, SavingSettingsCommitsOnce)
{
    Fakes::FakeStore store;

    ASSERT_TRUE(SaveSettings(store, Settings{}));

    EXPECT_EQ(store.commitCount, 1);   // one flash write for all the settings, not one per setting
}

TEST(SettingsTest, SaveReportsACommitFailure)
{
    Fakes::FakeStore store;
    store.failCommits = true;

    EXPECT_FALSE(SaveSettings(store, Settings{}));
}

TEST(SettingsTest, SaveReportsStoreFailure)
{
    Fakes::FakeStore store;
    store.failWrites = true;

    EXPECT_FALSE(SaveSettings(store, Settings{}));
}

TEST(SettingsTest, PumpConfigConvertsMinutesToSeconds)
{
    Settings s;
    s.pumpMaxTimeOnM = 2;
    s.pumpCooldownTimeM = 3;
    s.pumpMinRunS = 7;
    s.pumpMinRestartS = 8;

    const PumpConfig c = PumpConfigFrom(s);

    EXPECT_EQ(c.maxOnTimeS, 120u);
    EXPECT_EQ(c.cooldownS, 180u);
    EXPECT_EQ(c.minRunTimeS, 7u);
    EXPECT_EQ(c.minRestartIntervalS, 8u);
}

TEST(SettingsTest, LevelIsHeightMinusDistanceAndOffsetClamped)
{
    Settings s; // height 140
    EXPECT_FLOAT_EQ(LevelFromDistanceCm(40.0f, s), 100.0f);

    s.filteredDistanceOffsetCm = 5;
    EXPECT_FLOAT_EQ(LevelFromDistanceCm(40.0f, s), 95.0f);

    EXPECT_FLOAT_EQ(LevelFromDistanceCm(500.0f, s), 0.0f);
    EXPECT_FLOAT_EQ(LevelFromDistanceCm(0.0f, s), 135.0f);
    EXPECT_FLOAT_EQ(LevelFromDistanceCm(-50.0f, s), 140.0f);
}

#include "../support/Main.h"
