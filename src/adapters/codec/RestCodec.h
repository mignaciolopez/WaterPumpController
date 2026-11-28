#pragma once

#include <cstddef>
#include <string>

#include "core/Messages.h"

/// JSON bodies and status codes of the REST interface (see docs/rest-api.md).
/// Unlike the WebSocket protocol, REST has no id/ok envelope: the HTTP status says whether it worked.
namespace CE::Adapters::RestCodec
{
    /// Larger request bodies are rejected.
    constexpr size_t kMaxBodyBytes = 512;

    /// Body of POST /api/v1/pump: {"on":true}
    Core::ErrorCode DecodePumpBody(const char* body, size_t length, Core::Command& command);

    /// Body of PUT /api/v1/settings: {"minLevelCm":90,...}
    Core::ErrorCode DecodeSettingsBody(const char* body, size_t length, Core::Command& command);

    std::string EncodeState(const Core::PumpState& pump, bool levelValid, float levelCm,
                            const Core::SensorRange& sensor = {});
    std::string EncodeSettings(const Core::Settings& settings);
    std::string EncodeOk();                            // {"ok":true}
    std::string EncodeError(Core::ErrorCode code);     // {"ok":false,"err":"PUMP_COOLDOWN"}

    /// 200 for success, 4xx for refused requests, 500 for internal errors.
    int HttpStatus(Core::ErrorCode code);
}
