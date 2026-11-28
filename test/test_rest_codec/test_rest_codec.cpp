#include <gtest/gtest.h>

#include <ArduinoJson.h>

#include <string>

#include "adapters/codec/RestCodec.h"

using namespace CE;
using namespace CE::Core;
using namespace CE::Adapters::RestCodec;

namespace
{
    ErrorCode Pump(const std::string& body, Command& command)
    {
        return DecodePumpBody(body.data(), body.size(), command);
    }

    ErrorCode SettingsBody(const std::string& body, Command& command)
    {
        return DecodeSettingsBody(body.data(), body.size(), command);
    }

    JsonDocument Parse(const std::string& json)
    {
        JsonDocument doc;
        EXPECT_FALSE(deserializeJson(doc, json)) << json;
        return doc;
    }
}

TEST(RestCodecDecodeTest, PumpBodyOnAndOff)
{
    Command c;
    EXPECT_EQ(Pump(R"({"on":true})", c), ErrorCode::None);
    EXPECT_EQ(c.type, CommandType::PumpSet);
    EXPECT_TRUE(c.on);

    EXPECT_EQ(Pump(R"({"on":false})", c), ErrorCode::None);
    EXPECT_FALSE(c.on);
}

TEST(RestCodecDecodeTest, PumpBodyRejectsBadInput)
{
    Command c;
    EXPECT_EQ(Pump("", c), ErrorCode::BadRequest);
    EXPECT_EQ(Pump("{not json", c), ErrorCode::BadRequest);
    EXPECT_EQ(Pump("[true]", c), ErrorCode::BadRequest);
    EXPECT_EQ(Pump("{}", c), ErrorCode::BadRequest);
    EXPECT_EQ(Pump(R"({"on":1})", c), ErrorCode::BadRequest);
    EXPECT_EQ(Pump(R"({"on":"true"})", c), ErrorCode::BadRequest);
    EXPECT_EQ(DecodePumpBody(nullptr, 5, c), ErrorCode::BadRequest);
}

TEST(RestCodecDecodeTest, PumpBodyTooLargeIsRejected)
{
    Command c;
    const std::string big = R"({"on":true,"pad":")" + std::string(kMaxBodyBytes, 'x') + "\"}";

    EXPECT_EQ(Pump(big, c), ErrorCode::BadRequest);
}

TEST(RestCodecDecodeTest, SettingsBodyCollectsValues)
{
    Command c;
    ASSERT_EQ(SettingsBody(R"({"minLevelCm":90,"maxLevelCm":100,"utcOffsetMinutes":-300})", c), ErrorCode::None);

    EXPECT_EQ(c.type, CommandType::SettingsSet);
    ASSERT_EQ(c.valueCount, 3u);
    int64_t offset = 0;
    for (size_t i = 0; i < c.valueCount; ++i)
        if (std::string(c.values[i].name) == "utcOffsetMinutes") offset = c.values[i].value;
    EXPECT_EQ(offset, -300);
}

TEST(RestCodecDecodeTest, SettingsBodyRejectsBadInput)
{
    Command c;
    EXPECT_EQ(SettingsBody("", c), ErrorCode::BadRequest);
    EXPECT_EQ(SettingsBody("{}", c), ErrorCode::BadRequest);
    EXPECT_EQ(SettingsBody("[1]", c), ErrorCode::BadRequest);
    EXPECT_EQ(SettingsBody(R"({"minLevelCm":1.5})", c), ErrorCode::InvalidValue);
    EXPECT_EQ(SettingsBody(R"({"minLevelCm":"50"})", c), ErrorCode::InvalidValue);
    EXPECT_EQ(SettingsBody(R"({")" + std::string(40, 'a') + R"(":1})", c), ErrorCode::UnknownField);
}

TEST(RestCodecEncodeTest, StateHasPumpAndLevelWithoutAnEnvelope)
{
    PumpState pump;
    pump.isOn = true;

    const auto doc = Parse(EncodeState(pump, true, 80.04f));

    EXPECT_STREQ(doc["pump"]["mode"], "running");
    EXPECT_NEAR(doc["level"]["cm"].as<float>(), 80.0f, 0.001f);
    EXPECT_TRUE(doc["ok"].isNull());
    EXPECT_TRUE(doc["id"].isNull());
}

TEST(RestCodecEncodeTest, StateCarriesTheSensorRangeWhenKnown)
{
    const auto doc = Parse(EncodeState(PumpState{}, true, 80.0f, SensorRange{3.0f, 450.0f}));

    EXPECT_FLOAT_EQ(doc["sensor"]["minDistanceCm"].as<float>(), 3.0f);
    EXPECT_FLOAT_EQ(doc["sensor"]["maxDistanceCm"].as<float>(), 450.0f);
}

TEST(RestCodecEncodeTest, StateLeavesTheSensorOutWhenTheRangeIsUnknown)
{
    EXPECT_TRUE(Parse(EncodeState(PumpState{}, true, 80.0f))["sensor"].isNull());
}

TEST(RestCodecEncodeTest, UnknownLevelIsNull)
{
    EXPECT_TRUE(Parse(EncodeState(PumpState{}, false, 0.0f))["level"]["cm"].isNull());
}

TEST(RestCodecEncodeTest, SettingsAreAFlatObjectWithEveryField)
{
    const CE::Core::Settings settings;

    const auto doc = Parse(EncodeSettings(settings));

    for (size_t i = 0; i < SettingFieldCount(); ++i)
    {
        const auto& field = SettingFieldAt(i);
        EXPECT_EQ(doc[field.name].as<int>(), settings.*(field.member)) << field.name;
    }
}

TEST(RestCodecEncodeTest, OkAndErrorBodies)
{
    EXPECT_EQ(EncodeOk(), R"({"ok":true})");
    EXPECT_EQ(EncodeError(ErrorCode::PumpCooldown), R"({"ok":false,"err":"PUMP_COOLDOWN"})");
}

TEST(RestCodecEncodeTest, HttpStatusMapping)
{
    EXPECT_EQ(HttpStatus(ErrorCode::None), 200);
    EXPECT_EQ(HttpStatus(ErrorCode::BadRequest), 400);
    EXPECT_EQ(HttpStatus(ErrorCode::UnknownField), 400);
    EXPECT_EQ(HttpStatus(ErrorCode::InvalidValue), 422);
    EXPECT_EQ(HttpStatus(ErrorCode::PumpCooldown), 409);
    EXPECT_EQ(HttpStatus(ErrorCode::PumpTooSoon), 409);
    EXPECT_EQ(HttpStatus(ErrorCode::HighLevel), 409);
    EXPECT_EQ(HttpStatus(ErrorCode::PumpFault), 409);
    EXPECT_EQ(HttpStatus(ErrorCode::Internal), 500);
}

#include "../support/Main.h"
