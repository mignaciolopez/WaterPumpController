#include "adapters/sensors/DHTDevice.h"

namespace CE::Adapters
{
    DHTDevice::DHTDevice(const byte pin, const byte type) : _dht(pin, type)
    {
    }

    void DHTDevice::Begin()
    {
        _dht.begin();
    }

    Core::WeatherSample DHTDevice::Read()
    {
        // `force` makes the library read now instead of returning its cached value.
        const float humidity = _dht.readHumidity(true);
        const float temperatureC = _dht.readTemperature(false, true);

        // computeHeatIndex needs valid inputs; MakeWeatherSample rejects NaN and out-of-range values first.
        const float heatIndexC = std::isnan(temperatureC) || std::isnan(humidity)
                                     ? NAN
                                     : _dht.computeHeatIndex(temperatureC, humidity, false);

        return Core::MakeWeatherSample(temperatureC, humidity, heatIndexC);
    }
}
