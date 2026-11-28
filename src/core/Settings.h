#pragma once

#include <cstddef>
#include <cstdint>

#include "core/PumpController.h"
#include "core/ports/IStore.h"

namespace CE::Core
{
    /// All user-tunable settings. Every field is described once in kSettingFields (Settings.cpp),
    /// which drives validation, persistence and (later) the network protocol.
    struct Settings
    {
        // Tank levels, measured from the bottom of the tank (cm)
        int32_t heightCm                 = 140;  // tank height = max distance to measure
        int32_t criticalLevelCm          = 20;   // start pumping even at night at or below this level
        int32_t minLevelCm               = 50;   // start pumping at or below this level
        int32_t maxLevelCm               = 130;  // stop pumping at or above this level
        int32_t filteredDistanceOffsetCm = 0;    // distance sensor offset

        // Sampling
        int32_t radarDelayS              = 1;    // delay between distance measurements
        int32_t medianWindow             = 5;    // samples used for the median (1..15)

        // Pump
        int32_t pumpMinRunS              = 10;   // minimum time ON before a stop is accepted
        int32_t pumpMinRestartS          = 10;   // minimum time OFF before the next start
        int32_t pumpMaxTimeOnM           = 20;   // max continuous run, then forced stop + cooldown
        int32_t pumpCooldownTimeM        = 60;   // lockout after a max on-time stop

        // Night mode: outside the critical level, the pump is not started at night
        int32_t nightStartHour           = 22;
        int32_t nightEndHour             = 8;

        // Time zone: local time = UTC + offset (no daylight saving). Default -180 = UTC-3 (Argentina)
        int32_t utcOffsetMinutes         = -180;

        // Network watchdog
        int32_t watchdogDelayS           = 60;   // seconds between network checks
        int32_t maxReconnects            = 10;   // failed reconnect rounds before the device restarts
    };

    struct SettingField
    {
        const char*    name;
        int32_t Settings::* member;
        int32_t       min;
        int32_t       max;
    };

    struct SettingUpdate
    {
        const char* name;
        int64_t     value;
    };

    enum class SettingResult : uint8_t
    {
        Ok,
        UnknownField,
        OutOfRange,    // value outside the field's min/max
        Inconsistent   // violates a rule between fields (e.g. minLevel >= maxLevel)
    };

    /// Field table access.
    size_t SettingFieldCount();
    const SettingField& SettingFieldAt(size_t index);
    const SettingField* FindSettingField(const char* name);

    /// Reads one field by name. Returns false if the name is unknown.
    bool GetSetting(const Settings& settings, const char* name, int64_t& value);

    /// Cross-field consistency (field ranges are checked on write).
    /// Rule: critical < min < max <= height.
    bool IsConsistent(const Settings& settings);

    /// Applies all updates to a copy, validates the result, and commits only if everything is valid.
    /// On failure `settings` is unchanged.
    SettingResult SetSettings(Settings& settings, const SettingUpdate* updates, size_t count);
    SettingResult SetSetting(Settings& settings, const char* name, int64_t value);

    /// Persistence through IStore (keys: "set.<name>"). Load keeps defaults for missing/invalid data.
    bool SaveSettings(IStore& store, const Settings& settings);
    void LoadSettings(const IStore& store, Settings& settings);

    /// Pump rule parameters derived from the settings.
    PumpConfig PumpConfigFrom(const Settings& settings);

    /// Water level above the tank bottom from a measured distance (sensor on top, looking down).
    /// level = height - (distance + offset), clamped to 0..height.
    float LevelFromDistanceCm(float distanceCm, const Settings& settings);
}
