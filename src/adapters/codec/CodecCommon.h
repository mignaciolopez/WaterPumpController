#pragma once

#include <ArduinoJson.h>

#include "core/Messages.h"
#include "core/PumpController.h"
#include "core/Settings.h"
#include "core/ports/IWeatherSensor.h"

/// JSON shapes shared by every interface (WebSocket v1, REST): what a pump state, a level and the
/// settings look like, and how command data is read. One place, so the interfaces cannot drift apart.
namespace CE::Adapters::Codec
{
    void FillPump(JsonObject out, const Core::PumpState& pump);

    /// `cm` is null when the level is unknown. 0.1 cm resolution.
    void FillLevel(JsonObject out, bool valid, float cm);

    void FillSettings(JsonObject out, const Core::Settings& settings);

    /// What the distance sensor can measure: minDistanceCm and maxDistanceCm.
    void FillSensor(JsonObject out, const Core::SensorRange& range);

    /// Temperature, humidity and heat index (0.1 resolution); all three are null while there is no valid reading.
    void FillWeather(JsonObject out, const Core::WeatherSample& weather);

    /// Reads {"on": <bool>} into a PumpSet command (type and `on`; the id is left alone).
    Core::ErrorCode ReadPumpSet(JsonVariantConst data, Core::Command& command);

    /// Reads {"<setting>": <integer>, ...} into a SettingsSet command (type and values).
    Core::ErrorCode ReadSettingsSet(JsonVariantConst data, Core::Command& command);
}
