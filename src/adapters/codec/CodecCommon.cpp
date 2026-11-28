#include "adapters/codec/CodecCommon.h"

#include <cmath>
#include <cstring>

namespace CE::Adapters::Codec
{
    using namespace Core;

    namespace
    {
        const char* ModeName(const PumpMode mode)
        {
            switch (mode)
            {
                case PumpMode::Idle:     return "idle";
                case PumpMode::Running:  return "running";
                case PumpMode::Cooldown: return "cooldown";
                case PumpMode::Fault:    return "fault";
            }
            return "idle";
        }

        const char* FaultName(const PumpFault fault)
        {
            switch (fault)
            {
                case PumpFault::None:               return "none";
                case PumpFault::NoTime:             return "noTime";
                case PumpFault::SensorInconsistent: return "sensorInconsistent";
            }
            return "none";
        }
    }

    void FillPump(const JsonObject out, const PumpState& pump)
    {
        out["mode"] = ModeName(PumpModeOf(pump));
        out["on"] = pump.isOn;
        out["cooldown"] = pump.isOnCooldown;
        out["fault"] = FaultName(pump.fault);
        out["onAt"] = pump.timeStampOn;
        out["offAt"] = pump.timeStampOff;
        out["cooldownAt"] = pump.timeStampCooldown;
    }

    void FillLevel(const JsonObject out, const bool valid, const float cm)
    {
        if (valid) out["cm"] = std::round(cm * 10.0f) / 10.0f;
        else out["cm"] = nullptr;
    }

    void FillSettings(const JsonObject out, const Settings& settings)
    {
        for (size_t i = 0; i < SettingFieldCount(); ++i)
        {
            const SettingField& field = SettingFieldAt(i);
            out[field.name] = settings.*(field.member);
        }
    }

    void FillSensor(const JsonObject out, const SensorRange& range)
    {
        out["minDistanceCm"] = range.minDistanceCm;
        out["maxDistanceCm"] = range.maxDistanceCm;
    }

    void FillWeather(const JsonObject out, const WeatherSample& weather)
    {
        if (!weather.valid)
        {
            out["temperatureC"] = nullptr;
            out["humidity"] = nullptr;
            out["heatIndexC"] = nullptr;
            return;
        }

        out["temperatureC"] = std::round(weather.temperatureC * 10.0f) / 10.0f;
        out["humidity"] = std::round(weather.humidity * 10.0f) / 10.0f;
        out["heatIndexC"] = std::round(weather.heatIndexC * 10.0f) / 10.0f;
    }

    ErrorCode ReadPumpSet(const JsonVariantConst data, Command& command)
    {
        if (!data["on"].is<bool>()) return ErrorCode::BadRequest;

        command.type = CommandType::PumpSet;
        command.on = data["on"].as<bool>();
        return ErrorCode::None;
    }

    ErrorCode ReadSettingsSet(const JsonVariantConst data, Command& command)
    {
        if (!data.is<JsonObjectConst>()) return ErrorCode::BadRequest;

        command.type = CommandType::SettingsSet;
        command.valueCount = 0;

        for (const JsonPairConst pair : data.as<JsonObjectConst>())
        {
            if (command.valueCount >= Command::kMaxSettingValues) return ErrorCode::BadRequest;

            const char* name = pair.key().c_str();
            if (std::strlen(name) >= SettingValue::kMaxNameLength) return ErrorCode::UnknownField;
            if (!pair.value().is<long long>()) return ErrorCode::InvalidValue;

            SettingValue& value = command.values[command.valueCount++];
            std::strcpy(value.name, name);
            value.value = pair.value().as<long long>();
        }

        return command.valueCount == 0 ? ErrorCode::BadRequest : ErrorCode::None;
    }
}
