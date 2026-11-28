#include "adapters/codec/CodecV1.h"

#include <ArduinoJson.h>

#include <cstring>

#include "adapters/codec/CodecCommon.h"

namespace CE::Adapters::CodecV1
{
    using namespace Core;

    namespace
    {
        std::string Serialize(const JsonDocument& doc)
        {
            std::string out;
            serializeJson(doc, out);
            return out;
        }

        DecodeResult Fail(const ErrorCode error, const int32_t id = Command::kNoId)
        {
            DecodeResult result;
            result.error = error;
            result.command.id = id;
            return result;
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
            const ErrorCode error = Codec::ReadPumpSet(data, result.command);
            if (error != ErrorCode::None) return Fail(error, id);
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
            const ErrorCode error = Codec::ReadSettingsSet(data, result.command);
            if (error != ErrorCode::None) return Fail(error, id);
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

    std::string EncodeStateResponse(const int32_t id, const PumpState& pump, const bool levelValid, const float levelCm,
                                    const SensorRange& sensor)
    {
        JsonDocument doc;
        doc["id"] = id;
        doc["ok"] = true;
        const JsonObject data = doc["data"].to<JsonObject>();
        Codec::FillPump(data["pump"].to<JsonObject>(), pump);
        Codec::FillLevel(data["level"].to<JsonObject>(), levelValid, levelCm);
        if (sensor.IsKnown()) Codec::FillSensor(data["sensor"].to<JsonObject>(), sensor);
        return Serialize(doc);
    }

    std::string EncodeSettingsResponse(const int32_t id, const Settings& settings)
    {
        JsonDocument doc;
        doc["id"] = id;
        doc["ok"] = true;
        Codec::FillSettings(doc["data"].to<JsonObject>(), settings);
        return Serialize(doc);
    }

    std::string EncodePumpStateEvent(const PumpState& pump)
    {
        JsonDocument doc;
        doc["event"] = "pump.state";
        Codec::FillPump(doc["data"].to<JsonObject>(), pump);
        return Serialize(doc);
    }

    std::string EncodeTankLevelEvent(const bool levelValid, const float levelCm)
    {
        JsonDocument doc;
        doc["event"] = "tank.level";
        Codec::FillLevel(doc["data"].to<JsonObject>(), levelValid, levelCm);
        return Serialize(doc);
    }

    std::string EncodeSettingsChangedEvent(const Settings& settings)
    {
        JsonDocument doc;
        doc["event"] = "settings.changed";
        Codec::FillSettings(doc["data"].to<JsonObject>(), settings);
        return Serialize(doc);
    }
}
