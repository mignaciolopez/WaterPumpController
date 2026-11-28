#pragma once

#include <Arduino.h>
#include <DHT.h>

#include "core/ports/IWeatherSensor.h"

namespace CE::Adapters
{
    /// DHT11 / DHT22 temperature and humidity sensor on one GPIO pin (Adafruit DHT library).
    ///
    /// The sensor needs about 1-2 seconds between reads (DHT11) or 2 seconds (DHT22); the caller sets the pace.
    class DHTDevice final : public Core::IWeatherSensor
    {
    public:
        /// `type` is the library's DHT11 or DHT22 constant.
        DHTDevice(byte pin, byte type);

        void Begin();

        Core::WeatherSample Read() override;

    private:
        DHT _dht;
    };
}
