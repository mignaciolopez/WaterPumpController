#include <gtest/gtest.h>

#include "app/Controller.h"
#include "core/ports/IDistanceSensor.h"

using namespace CE;
using namespace CE::Core;

namespace
{
    constexpr float kMin = 3.0f;
    constexpr float kMax = 450.0f;
}

TEST(ClassifyDistanceTest, AMeasurementInsideTheRangeIsOk)
{
    const auto r = ClassifyDistance(42.2f, kMin, kMax);

    EXPECT_EQ(r.status, DistanceStatus::Ok);
    EXPECT_FLOAT_EQ(r.cm, 42.2f);
}

TEST(ClassifyDistanceTest, TheLimitsThemselvesAreOk)
{
    EXPECT_EQ(ClassifyDistance(3.0f, kMin, kMax).status, DistanceStatus::Ok);
    EXPECT_EQ(ClassifyDistance(450.0f, kMin, kMax).status, DistanceStatus::Ok);
}

TEST(ClassifyDistanceTest, CloserThanTheMinimumIsTooCloseAndClampedToTheMinimum)
{
    const auto r = ClassifyDistance(1.2f, kMin, kMax);

    EXPECT_EQ(r.status, DistanceStatus::TooClose);
    EXPECT_FLOAT_EQ(r.cm, kMin);
}

TEST(ClassifyDistanceTest, BeyondTheMaximumIsOutOfRange)
{
    EXPECT_EQ(ClassifyDistance(450.1f, kMin, kMax).status, DistanceStatus::OutOfRange);
}

TEST(ToSampleTest, OkAndTooCloseBecomeValidSamples)
{
    const auto ok = App::ToSample({DistanceStatus::Ok, 42.2f}, kMin);
    EXPECT_TRUE(ok.valid);
    EXPECT_FLOAT_EQ(ok.cm, 42.2f);
    EXPECT_FALSE(ok.silent);

    const auto close = App::ToSample({DistanceStatus::TooClose, kMin}, kMin);
    EXPECT_TRUE(close.valid);
    EXPECT_FLOAT_EQ(close.cm, kMin);
}

TEST(ToSampleTest, NoEchoBecomesASilentSampleCarryingTheBlindZone)
{
    const auto s = App::ToSample({DistanceStatus::NoEcho, 0.0f}, kMin);

    EXPECT_FALSE(s.valid);
    EXPECT_TRUE(s.silent);
    EXPECT_FLOAT_EQ(s.blindZoneCm, kMin);
}

TEST(ToSampleTest, OutOfRangeIsAPlainFailure)
{
    const auto s = App::ToSample({DistanceStatus::OutOfRange, 0.0f}, kMin);

    EXPECT_FALSE(s.valid);
    EXPECT_FALSE(s.silent);
}

#include "../support/Main.h"
