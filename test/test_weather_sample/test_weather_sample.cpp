#include <gtest/gtest.h>

#include <cmath>

#include "core/ports/IWeatherSensor.h"

using namespace CE::Core;

TEST(MakeWeatherSampleTest, ATypicalReadingIsValid)
{
    const auto s = MakeWeatherSample(23.4f, 61.0f, 24.0f);

    EXPECT_TRUE(s.valid);
    EXPECT_FLOAT_EQ(s.temperatureC, 23.4f);
    EXPECT_FLOAT_EQ(s.humidity, 61.0f);
    EXPECT_FLOAT_EQ(s.heatIndexC, 24.0f);
}

TEST(MakeWeatherSampleTest, NaNValuesAreInvalid)
{
    EXPECT_FALSE(MakeWeatherSample(NAN, 50.0f, 20.0f).valid);
    EXPECT_FALSE(MakeWeatherSample(20.0f, NAN, 20.0f).valid);
}

TEST(MakeWeatherSampleTest, HumidityOutsideZeroToHundredIsInvalid)
{
    EXPECT_FALSE(MakeWeatherSample(20.0f, -0.1f, 20.0f).valid);
    EXPECT_FALSE(MakeWeatherSample(20.0f, 100.1f, 20.0f).valid);
    EXPECT_TRUE(MakeWeatherSample(20.0f, 0.0f, 20.0f).valid);
    EXPECT_TRUE(MakeWeatherSample(20.0f, 100.0f, 20.0f).valid);
}

TEST(MakeWeatherSampleTest, ImplausibleTemperaturesAreInvalid)
{
    EXPECT_FALSE(MakeWeatherSample(-40.1f, 50.0f, 0.0f).valid);
    EXPECT_FALSE(MakeWeatherSample(80.1f, 50.0f, 0.0f).valid);
    EXPECT_TRUE(MakeWeatherSample(-40.0f, 50.0f, 0.0f).valid);
    EXPECT_TRUE(MakeWeatherSample(80.0f, 50.0f, 0.0f).valid);
}

TEST(MakeWeatherSampleTest, AMissingHeatIndexFallsBackToTheTemperature)
{
    const auto s = MakeWeatherSample(21.5f, 40.0f, NAN);

    EXPECT_TRUE(s.valid);
    EXPECT_FLOAT_EQ(s.heatIndexC, 21.5f);
}

TEST(MakeWeatherSampleTest, AnInvalidSampleCarriesNoValues)
{
    const auto s = MakeWeatherSample(NAN, 50.0f, 20.0f);

    EXPECT_FALSE(s.valid);
    EXPECT_FLOAT_EQ(s.temperatureC, 0.0f);
    EXPECT_FLOAT_EQ(s.humidity, 0.0f);
}

#include "../support/Main.h"
