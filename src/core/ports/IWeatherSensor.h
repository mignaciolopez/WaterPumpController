#pragma once

#include <cmath>

namespace CE::Core
{
    /// One temperature / humidity reading. `valid` is false when the sensor gave nothing usable.
    struct WeatherSample
    {
        bool  valid = false;
        float temperatureC = 0.0f;
        float humidity = 0.0f;      // relative humidity, percent
        float heatIndexC = 0.0f;    // "feels like" temperature
    };

    /// Builds a sample from raw values, marking it invalid when they are not plausible
    /// (NaN, humidity outside 0..100 %, temperature outside -40..80 C).
    inline WeatherSample MakeWeatherSample(const float temperatureC, const float humidity, const float heatIndexC)
    {
        WeatherSample sample;
        sample.valid = !std::isnan(temperatureC) && !std::isnan(humidity) && humidity >= 0.0f && humidity <= 100.0f &&
                       temperatureC >= -40.0f && temperatureC <= 80.0f;
        if (!sample.valid) return sample;

        sample.temperatureC = temperatureC;
        sample.humidity = humidity;
        sample.heatIndexC = std::isnan(heatIndexC) ? temperatureC : heatIndexC;
        return sample;
    }

    /// A temperature / humidity sensor (DHT11, DHT22...). Reading may block for a few milliseconds.
    class IWeatherSensor
    {
    public:
        virtual ~IWeatherSensor() = default;

        /// Takes one reading. `valid` is false on a failed or implausible reading.
        virtual WeatherSample Read() = 0;
    };
}
