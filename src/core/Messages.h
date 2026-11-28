#pragma once

#include <cstddef>
#include <cstdint>

#include "core/PumpController.h"
#include "core/Settings.h"

namespace CE::Core
{
    /// Stable error codes shared by every interface (see docs/protocol.md).
    enum class ErrorCode : uint8_t
    {
        None,
        BadRequest,      // malformed message or wrong field types
        UnknownCommand,
        UnknownField,    // unknown setting name
        InvalidValue,    // out of range or violates a rule between settings
        PumpCooldown,
        PumpTooSoon,
        HighLevel,
        PumpFault,
        Internal
    };

    /// Wire name of an error code, e.g. "PUMP_COOLDOWN". "" for None.
    const char* ErrorCodeName(ErrorCode code);

    ErrorCode ToErrorCode(PumpResult result);
    ErrorCode ToErrorCode(SettingResult result);

    /// What the distance sensor can measure, so a client can scale its display. 0/0 = not known (left out).
    /// The level is `heightCm - distance`, so it ranges from `max(0, heightCm - maxDistanceCm)` up to
    /// `heightCm - minDistanceCm`.
    struct SensorRange
    {
        float minDistanceCm = 0.0f;
        float maxDistanceCm = 0.0f;

        bool IsKnown() const { return maxDistanceCm > 0.0f; }
    };

    enum class CommandType : uint8_t
    {
        PumpSet,      // turn the pump on or off
        StateGet,     // snapshot of pump and level
        SettingsGet,
        SettingsSet   // atomic update of one or more settings
    };

    struct SettingValue
    {
        static constexpr size_t kMaxNameLength = 32;

        char    name[kMaxNameLength] = {};
        int64_t value = 0;
    };

    /// A decoded request, independent of the wire format.
    struct Command
    {
        // Room for every setting plus some to grow (a client may send all of them back at once).
        // test_settings fails when the settings outgrow it.
        static constexpr size_t kMaxSettingValues = 24;
        static constexpr int32_t kNoId = -1;

        CommandType  type = CommandType::StateGet;
        int32_t      id = kNoId;     // chosen by the client; echoed in the response
        bool         on = false;     // PumpSet
        SettingValue values[kMaxSettingValues];  // SettingsSet
        size_t       valueCount = 0;
    };
}
