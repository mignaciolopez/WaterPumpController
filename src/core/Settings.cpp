#include "core/Settings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace CE::Core
{
    namespace
    {
        constexpr SettingField kSettingFields[] = {
            {"heightCm",                  &Settings::heightCm,                 20,  450},
            {"criticalLevelCm",           &Settings::criticalLevelCm,          0,   450},
            {"minLevelCm",                &Settings::minLevelCm,               0,   450},
            {"maxLevelCm",                &Settings::maxLevelCm,               0,   450},
            {"filteredDistanceOffsetCm",  &Settings::filteredDistanceOffsetCm, 0,   100},
            {"radarDelayS",               &Settings::radarDelayS,              1,   3600},
            {"medianWindow",              &Settings::medianWindow,             1,   15},
            {"pumpMinRunS",               &Settings::pumpMinRunS,              0,   600},
            {"pumpMinRestartS",           &Settings::pumpMinRestartS,          0,   600},
            {"pumpMaxTimeOnM",            &Settings::pumpMaxTimeOnM,           1,   240},
            {"pumpCooldownTimeM",         &Settings::pumpCooldownTimeM,        1,   1440},
            {"nightStartHour",            &Settings::nightStartHour,           0,   23},
            {"nightEndHour",              &Settings::nightEndHour,             0,   23},
        };

        constexpr size_t kFieldCount = sizeof(kSettingFields) / sizeof(kSettingFields[0]);

        bool BuildKey(char* buffer, const size_t size, const char* name)
        {
            const int written = std::snprintf(buffer, size, "set.%s", name);
            return written > 0 && static_cast<size_t>(written) < size;
        }
    }

    size_t SettingFieldCount()
    {
        return kFieldCount;
    }

    const SettingField& SettingFieldAt(const size_t index)
    {
        return kSettingFields[index];
    }

    const SettingField* FindSettingField(const char* name)
    {
        if (name == nullptr) return nullptr;

        for (const auto& field : kSettingFields)
            if (std::strcmp(field.name, name) == 0) return &field;

        return nullptr;
    }

    bool GetSetting(const Settings& settings, const char* name, int64_t& value)
    {
        const SettingField* field = FindSettingField(name);
        if (field == nullptr) return false;

        value = settings.*(field->member);
        return true;
    }

    bool IsConsistent(const Settings& settings)
    {
        return settings.criticalLevelCm < settings.minLevelCm &&
               settings.minLevelCm < settings.maxLevelCm &&
               settings.maxLevelCm <= settings.heightCm;
    }

    SettingResult SetSettings(Settings& settings, const SettingUpdate* updates, const size_t count)
    {
        Settings candidate = settings;

        for (size_t i = 0; i < count; ++i)
        {
            const SettingField* field = FindSettingField(updates[i].name);
            if (field == nullptr) return SettingResult::UnknownField;
            if (updates[i].value < field->min || updates[i].value > field->max) return SettingResult::OutOfRange;

            candidate.*(field->member) = static_cast<uint16_t>(updates[i].value);
        }

        if (!IsConsistent(candidate)) return SettingResult::Inconsistent;

        settings = candidate;
        return SettingResult::Ok;
    }

    SettingResult SetSetting(Settings& settings, const char* name, const int64_t value)
    {
        const SettingUpdate update{name, value};
        return SetSettings(settings, &update, 1);
    }

    bool SaveSettings(IStore& store, const Settings& settings)
    {
        bool ok = true;
        char key[48];

        for (const auto& field : kSettingFields)
        {
            if (!BuildKey(key, sizeof(key), field.name) || !store.SetI64(key, settings.*(field.member)))
                ok = false;
        }

        return ok;
    }

    void LoadSettings(const IStore& store, Settings& settings)
    {
        Settings candidate = settings;
        char key[48];

        for (const auto& field : kSettingFields)
        {
            int64_t value = 0;
            if (!BuildKey(key, sizeof(key), field.name) || !store.GetI64(key, value)) continue;
            if (value < field.min || value > field.max) continue;

            candidate.*(field.member) = static_cast<uint16_t>(value);
        }

        if (IsConsistent(candidate)) settings = candidate;
    }

    PumpConfig PumpConfigFrom(const Settings& settings)
    {
        PumpConfig config;
        config.minRunTimeS         = settings.pumpMinRunS;
        config.minRestartIntervalS = settings.pumpMinRestartS;
        config.maxOnTimeS          = static_cast<uint32_t>(settings.pumpMaxTimeOnM) * 60;
        config.cooldownS           = static_cast<uint32_t>(settings.pumpCooldownTimeM) * 60;
        return config;
    }

    float LevelFromDistanceCm(const float distanceCm, const Settings& settings)
    {
        const float level = static_cast<float>(settings.heightCm) - (distanceCm + settings.filteredDistanceOffsetCm);
        return std::clamp(level, 0.0f, static_cast<float>(settings.heightCm));
    }
}
