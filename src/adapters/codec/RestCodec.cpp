#include "adapters/codec/RestCodec.h"

#include <ArduinoJson.h>

#include "adapters/codec/CodecCommon.h"

namespace CE::Adapters::RestCodec
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

        bool ParseObject(const char* body, const size_t length, JsonDocument& doc)
        {
            if (body == nullptr || length == 0 || length > kMaxBodyBytes) return false;
            return !deserializeJson(doc, body, length) && doc.is<JsonObject>();
        }
    }

    ErrorCode DecodePumpBody(const char* body, const size_t length, Command& command)
    {
        JsonDocument doc;
        if (!ParseObject(body, length, doc)) return ErrorCode::BadRequest;
        return Codec::ReadPumpSet(doc, command);
    }

    ErrorCode DecodeSettingsBody(const char* body, const size_t length, Command& command)
    {
        JsonDocument doc;
        if (!ParseObject(body, length, doc)) return ErrorCode::BadRequest;
        return Codec::ReadSettingsSet(doc, command);
    }

    std::string EncodeState(const PumpState& pump, const bool levelValid, const float levelCm, const SensorRange& sensor,
                            const WeatherSample* weather)
    {
        JsonDocument doc;
        Codec::FillPump(doc["pump"].to<JsonObject>(), pump);
        Codec::FillLevel(doc["level"].to<JsonObject>(), levelValid, levelCm);
        if (sensor.IsKnown()) Codec::FillSensor(doc["sensor"].to<JsonObject>(), sensor);
        if (weather != nullptr) Codec::FillWeather(doc["weather"].to<JsonObject>(), *weather);
        return Serialize(doc);
    }

    std::string EncodeSettings(const Settings& settings)
    {
        JsonDocument doc;
        Codec::FillSettings(doc.to<JsonObject>(), settings);
        return Serialize(doc);
    }

    std::string EncodeOk()
    {
        return R"({"ok":true})";
    }

    std::string EncodeError(const ErrorCode code)
    {
        JsonDocument doc;
        doc["ok"] = false;
        doc["err"] = ErrorCodeName(code);
        return Serialize(doc);
    }

    int HttpStatus(const ErrorCode code)
    {
        switch (code)
        {
            case ErrorCode::None:           return 200;
            case ErrorCode::BadRequest:     return 400;
            case ErrorCode::UnknownField:   return 400;
            case ErrorCode::InvalidValue:   return 422;
            case ErrorCode::UnknownCommand: return 404;
            case ErrorCode::PumpCooldown:   return 409;
            case ErrorCode::PumpTooSoon:    return 409;
            case ErrorCode::HighLevel:      return 409;
            case ErrorCode::PumpFault:      return 409;
            case ErrorCode::Internal:       return 500;
        }
        return 500;
    }
}
