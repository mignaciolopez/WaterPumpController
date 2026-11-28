#include "adapters/codec/CodecV1.h"

#include <ArduinoJson.h>

#include <cmath>
#include <cstring>

namespace CE::Adapters::CodecV1
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

        std::string Serialize(const JsonDocument& doc)
        {
            std::string out;
            serializeJson(doc, out);
            return out;
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
            if (valid) out["cm"] = std::round(cm * 10.0f) / 10.0f;  // 0.1 cm resolution
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

        DecodeResult Fail(const ErrorCode error, const int32_t id = Command::kNoId)
        {
            DecodeResult result;
            result.error = error;
            result.command.id = id;
            return result;
        }

        bool ReadSettingValues(const JsonVariantConst data, Command& command, ErrorCode& error)
        {
            if (!data.is<JsonObjectConst>())
            {
                error = ErrorCode::BadRequest;
                return false;
            }

            for (const JsonPairConst pair : data.as<JsonObjectConst>())
            {
                if (command.valueCount >= Command::kMaxSettingValues)
                {
                    error = ErrorCode::BadRequest;
                    return false;
                }

                const char* name = pair.key().c_str();
                if (std::strlen(name) >= SettingValue::kMaxNameLength)
                {
                    error = ErrorCode::UnknownField;
                    return false;
                }
                if (!pair.value().is<long long>())
                {
                    error = ErrorCode::InvalidValue;
                    return false;
                }

                SettingValue& value = command.values[command.valueCount++];
                std::strcpy(value.name, name);
                value.value = pair.value().as<long long>();
            }

            if (command.valueCount == 0)
            {
                error = ErrorCode::BadRequest;
                return false;
            }
            return true;
        }
    }

    DecodeResult DecodeCommand(const char* json, const size_t length)
    {
        if (json == nullptr || length == 0 || length > kMaxMessageBytes) return Fail(ErrorCode::BadRequest);

        JsonDocument doc;
        if (deserializeJson(doc, json, length) || !doc.is<JsonObject>()) return Fail(ErrorCode::BadRequest);

        const JsonVariantConst idVariant = doc["id"];
        if (!idVariant.is<int32_t>() || idVariant.as<int32_t>() < 0) return Fail(ErrorCode::BadRequest);
        const int32_t id = idVariant.as<int32_t>();

        if (!doc["cmd"].is<const char*>()) return Fail(ErrorCode::BadRequest, id);
        const char* name = doc["cmd"].as<const char*>();
        const JsonVariantConst data = doc["data"];

        DecodeResult result;
        result.command.id = id;

        if (std::strcmp(name, "pump.set") == 0)
        {
            if (!data["on"].is<bool>()) return Fail(ErrorCode::BadRequest, id);
            result.command.type = CommandType::PumpSet;
            result.command.on = data["on"].as<bool>();
        }
        else if (std::strcmp(name, "state.get") == 0)
        {
            result.command.type = CommandType::StateGet;
        }
        else if (std::strcmp(name, "settings.get") == 0)
        {
            result.command.type = CommandType::SettingsGet;
        }
        else if (std::strcmp(name, "settings.set") == 0)
        {
            result.command.type = CommandType::SettingsSet;
            ErrorCode error = ErrorCode::None;
            if (!ReadSettingValues(data, result.command, error)) return Fail(error, id);
        }
        else
        {
            return Fail(ErrorCode::UnknownCommand, id);
        }

        return result;
    }

    std::string EncodeOk(const int32_t id)
    {
        JsonDocument doc;
        doc["id"] = id;
        doc["ok"] = true;
        return Serialize(doc);
    }

    std::string EncodeError(const int32_t id, const ErrorCode code)
    {
        JsonDocument doc;
        if (id == Command::kNoId) doc["id"] = nullptr;
        else doc["id"] = id;
        doc["ok"] = false;
        doc["err"] = ErrorCodeName(code);
        return Serialize(doc);
    }

    std::string EncodeStateResponse(const int32_t id, const PumpState& pump, const bool levelValid, const float levelCm)
    {
        JsonDocument doc;
        doc["id"] = id;
        doc["ok"] = true;
        const JsonObject data = doc["data"].to<JsonObject>();
        FillPump(data["pump"].to<JsonObject>(), pump);
        FillLevel(data["level"].to<JsonObject>(), levelValid, levelCm);
        return Serialize(doc);
    }

    std::string EncodeSettingsResponse(const int32_t id, const Settings& settings)
    {
        JsonDocument doc;
        doc["id"] = id;
        doc["ok"] = true;
        FillSettings(doc["data"].to<JsonObject>(), settings);
        return Serialize(doc);
    }

    std::string EncodePumpStateEvent(const PumpState& pump)
    {
        JsonDocument doc;
        doc["event"] = "pump.state";
        FillPump(doc["data"].to<JsonObject>(), pump);
        return Serialize(doc);
    }

    std::string EncodeTankLevelEvent(const bool levelValid, const float levelCm)
    {
        JsonDocument doc;
        doc["event"] = "tank.level";
        FillLevel(doc["data"].to<JsonObject>(), levelValid, levelCm);
        return Serialize(doc);
    }

    std::string EncodeSettingsChangedEvent(const Settings& settings)
    {
        JsonDocument doc;
        doc["event"] = "settings.changed";
        FillSettings(doc["data"].to<JsonObject>(), settings);
        return Serialize(doc);
    }
}
