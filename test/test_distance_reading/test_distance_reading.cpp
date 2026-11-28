#include <gtest/gtest.h>

#include "app/Controller.h"
#include "core/DistanceInterpreter.h"

using namespace CE;
using namespace CE::Core;

namespace
{
    constexpr float kMin = 3.0f;
    constexpr float kMax = 450.0f;   // with the 2 cm margin: "near the minimum" is <= 5, "near the maximum" is >= 448

    DistanceInterpreter MakeInterpreter()
    {
        return DistanceInterpreter(kMin, kMax);
    }
}

// ---------------------------------------------------------------- readings with an echo

TEST(DistanceInterpreterTest, AMeasurementInTheMiddleOfTheRangeIsOk)
{
    auto interpreter = MakeInterpreter();

    const auto r = interpreter.Interpret(true, 42.2f);

    EXPECT_EQ(r.status, DistanceStatus::Ok);
    EXPECT_FLOAT_EQ(r.cm, 42.2f);
    EXPECT_FALSE(r.inferred);
}

TEST(DistanceInterpreterTest, WithinTheMarginOfTheMinimumIsTooCloseAndKeepsItsRealValue)
{
    auto interpreter = MakeInterpreter();

    const auto r = interpreter.Interpret(true, 4.2f);

    EXPECT_EQ(r.status, DistanceStatus::TooClose);
    EXPECT_FLOAT_EQ(r.cm, 4.2f);
    EXPECT_FALSE(r.inferred);
}

TEST(DistanceInterpreterTest, TheEdgeOfTheMarginIsIncluded)
{
    EXPECT_EQ(MakeInterpreter().Interpret(true, 5.0f).status, DistanceStatus::TooClose);   // minimum 3 + margin 2
    EXPECT_EQ(MakeInterpreter().Interpret(true, 5.1f).status, DistanceStatus::Ok);
    EXPECT_EQ(MakeInterpreter().Interpret(true, 447.9f).status, DistanceStatus::Ok);
    EXPECT_EQ(MakeInterpreter().Interpret(true, 448.0f).status, DistanceStatus::OutOfRange);  // maximum 450 - margin 2
}

TEST(DistanceInterpreterTest, ANoisyValueBelowTheMinimumIsClampedToIt)
{
    EXPECT_FLOAT_EQ(MakeInterpreter().Interpret(true, 1.0f).cm, kMin);
}

TEST(DistanceInterpreterTest, WithinTheMarginOfTheMaximumIsOutOfRange)
{
    const auto r = MakeInterpreter().Interpret(true, 449.0f);

    EXPECT_EQ(r.status, DistanceStatus::OutOfRange);
    EXPECT_FLOAT_EQ(r.cm, 449.0f);
}

// ---------------------------------------------------------------- silence

TEST(DistanceInterpreterTest, SilenceAfterAReadingNearTheMinimumMeansTheWaterIsInTheBlindZone)
{
    auto interpreter = MakeInterpreter();
    interpreter.Interpret(true, 6.0f);
    interpreter.Interpret(true, 4.5f);   // the last reading before the sensor goes quiet

    const auto r = interpreter.Interpret(false, 0.0f);

    EXPECT_EQ(r.status, DistanceStatus::TooClose);
    EXPECT_FLOAT_EQ(r.cm, kMin);
    EXPECT_TRUE(r.inferred);
}

TEST(DistanceInterpreterTest, TheBlindZoneStaysWhileTheSilenceLasts)
{
    auto interpreter = MakeInterpreter();
    interpreter.Interpret(true, 4.5f);

    for (int i = 0; i < 20; ++i)
    {
        const auto r = interpreter.Interpret(false, 0.0f);
        EXPECT_EQ(r.status, DistanceStatus::TooClose) << i;
        EXPECT_TRUE(r.inferred);
    }
}

TEST(DistanceInterpreterTest, ARealReadingEndsTheBlindZone)
{
    auto interpreter = MakeInterpreter();
    interpreter.Interpret(true, 4.5f);
    interpreter.Interpret(false, 0.0f);

    EXPECT_EQ(interpreter.Interpret(true, 30.0f).status, DistanceStatus::Ok);

    const auto afterwards = interpreter.Interpret(false, 0.0f);   // silence now has no explanation
    EXPECT_EQ(afterwards.status, DistanceStatus::NoEcho);
    EXPECT_FALSE(afterwards.inferred);
}

TEST(DistanceInterpreterTest, SilenceAfterANormalReadingIsAFailure)
{
    auto interpreter = MakeInterpreter();
    interpreter.Interpret(true, 90.0f);

    const auto r = interpreter.Interpret(false, 0.0f);

    EXPECT_EQ(r.status, DistanceStatus::NoEcho);
    EXPECT_FALSE(r.inferred);
}

TEST(DistanceInterpreterTest, SilenceWithoutAnyHistoryIsAFailure)
{
    EXPECT_EQ(MakeInterpreter().Interpret(false, 0.0f).status, DistanceStatus::NoEcho);
}

TEST(DistanceInterpreterTest, SilenceAfterAReadingNearTheMaximumMeansBeyondTheFarLimit)
{
    auto interpreter = MakeInterpreter();
    interpreter.Interpret(true, 449.0f);

    const auto r = interpreter.Interpret(false, 0.0f);

    EXPECT_EQ(r.status, DistanceStatus::OutOfRange);
    EXPECT_FLOAT_EQ(r.cm, kMax);
    EXPECT_TRUE(r.inferred);
}

// ---------------------------------------------------------------- from reading to sample

TEST(ToSampleTest, OkAndTooCloseBecomeValidSamples)
{
    const auto ok = App::ToSample({DistanceStatus::Ok, 42.2f, false}, kMin);
    EXPECT_TRUE(ok.valid);
    EXPECT_FLOAT_EQ(ok.cm, 42.2f);
    EXPECT_FALSE(ok.inferred);

    const auto close = App::ToSample({DistanceStatus::TooClose, 4.2f, false}, kMin);
    EXPECT_TRUE(close.valid);
    EXPECT_FLOAT_EQ(close.cm, 4.2f);
    EXPECT_FALSE(close.inferred);
}

TEST(ToSampleTest, AnInferredReadingIsMarked)
{
    const auto s = App::ToSample({DistanceStatus::TooClose, kMin, true}, kMin);

    EXPECT_TRUE(s.valid);
    EXPECT_TRUE(s.inferred);
    EXPECT_FLOAT_EQ(s.cm, kMin);
}

TEST(ToSampleTest, NoEchoBecomesASilentSampleCarryingTheBlindZone)
{
    const auto s = App::ToSample({DistanceStatus::NoEcho, 0.0f, false}, kMin);

    EXPECT_FALSE(s.valid);
    EXPECT_TRUE(s.silent);
    EXPECT_FLOAT_EQ(s.blindZoneCm, kMin);
}

TEST(ToSampleTest, OutOfRangeIsAPlainFailure)
{
    const auto s = App::ToSample({DistanceStatus::OutOfRange, 449.0f, false}, kMin);

    EXPECT_FALSE(s.valid);
    EXPECT_FALSE(s.silent);
}

#include "../support/Main.h"
