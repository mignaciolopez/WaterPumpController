#pragma once

#include <ArduinoJson.h>

#include "core/Messages.h"
#include "core/PumpController.h"
#include "core/Settings.h"

/// JSON shapes shared by every interface (WebSocket v1, REST): what a pump state, a level and the
/// settings look like, and how command data is read. One place, so the interfaces cannot drift apart.
namespace CE::Adapters::Codec
{
    void FillPump(JsonObject out, const Core::PumpState& pump);

    /// `cm` is null when the level is unknown. 0.1 cm resolution.
    void FillLevel(JsonObject out, bool valid, float cm);

    void FillSettings(JsonObject out, const Core::Settings& settings);

    /// Reads {"on": <bool>} into a PumpSet command (type and `on`; the id is left alone).
    Core::ErrorCode ReadPumpSet(JsonVariantConst data, Core::Command& command);

    /// Reads {"<setting>": <integer>, ...} into a SettingsSet command (type and values).
    Core::ErrorCode ReadSettingsSet(JsonVariantConst data, Core::Command& command);
}
