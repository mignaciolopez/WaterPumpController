#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/Messages.h"

/// Protocol v1 (WebSocket path /ws/v1). See docs/protocol.md.
/// The version is part of the endpoint, so messages carry no version field.
namespace CE::Adapters::CodecV1
{
    /// Larger messages are rejected as BAD_REQUEST.
    constexpr size_t kMaxMessageBytes = 1024;

    struct DecodeResult
    {
        Core::ErrorCode error = Core::ErrorCode::None;

        /// On success the full command. On failure only `id` may be set
        /// (Command::kNoId when the message had no usable id), so an error response can echo it.
        Core::Command command;
    };

    DecodeResult DecodeCommand(const char* json, size_t length);

    // Responses (one per command)
    std::string EncodeOk(int32_t id);
    std::string EncodeError(int32_t id, Core::ErrorCode code);
    std::string EncodeStateResponse(int32_t id, const Core::PumpState& pump, bool levelValid, float levelCm,
                                    const Core::SensorRange& sensor = {});
    std::string EncodeSettingsResponse(int32_t id, const Core::Settings& settings);

    // Events (pushed to clients)
    std::string EncodePumpStateEvent(const Core::PumpState& pump);
    std::string EncodeTankLevelEvent(bool levelValid, float levelCm);
    std::string EncodeSettingsChangedEvent(const Core::Settings& settings);
}
