#include <gtest/gtest.h>

#include <ArduinoJson.h>

#include <string>

#include "adapters/codec/CodecV1.h"

using namespace CE;
using namespace CE::Core;
using namespace CE::Adapters::CodecV1;

namespace
{
    DecodeResult Decode(const std::string& json)
    {
        return DecodeCommand(json.data(), json.size());
    }

    JsonDocument Parse(const std::string& json)
    {
        JsonDocument doc;
        EXPECT_FALSE(deserializeJson(doc, json)) << json;
        return doc;
    }
}

// ---------------------------------------------------------------- decoding

TEST(CodecV1DecodeTest, PumpSetOn)
{
    const auto r = Decode(R"({"cmd":"pump.set","id":7,"data":{"on":true}})");

    EXPECT_EQ(r.error, ErrorCode::None);
    EXPECT_EQ(r.command.type, CommandType::PumpSet);
    EXPECT_EQ(r.command.id, 7);
    EXPECT_TRUE(r.command.on);
}

TEST(CodecV1DecodeTest, PumpSetOff)
{
    const auto r = Decode(R"({"cmd":"pump.set","id":8,"data":{"on":false}})");

    EXPECT_EQ(r.error, ErrorCode::None);
    EXPECT_FALSE(r.command.on);
}

TEST(CodecV1DecodeTest, StateAndSettingsGetNeedNoData)
{
    const auto state = Decode(R"({"cmd":"state.get","id":1})");
    EXPECT_EQ(state.error, ErrorCode::None);
    EXPECT_EQ(state.command.type, CommandType::StateGet);

    const auto settings = Decode(R"({"cmd":"settings.get","id":2})");
    EXPECT_EQ(settings.error, ErrorCode::None);
    EXPECT_EQ(settings.command.type, CommandType::SettingsGet);
}

TEST(CodecV1DecodeTest, SettingsSetCollectsAllValues)
{
    const auto r = Decode(R"({"cmd":"settings.set","id":3,"data":{"minLevelCm":90,"maxLevelCm":100}})");

    ASSERT_EQ(r.error, ErrorCode::None);
    EXPECT_EQ(r.command.type, CommandType::SettingsSet);
    ASSERT_EQ(r.command.valueCount, 2u);

    int64_t min = -1, max = -1;
    for (size_t i = 0; i < r.command.valueCount; ++i)
    {
        if (std::string(r.command.values[i].name) == "minLevelCm") min = r.command.values[i].value;
        if (std::string(r.command.values[i].name) == "maxLevelCm") max = r.command.values[i].value;
    }
    EXPECT_EQ(min, 90);
    EXPECT_EQ(max, 100);
}

TEST(CodecV1DecodeTest, MalformedJsonIsBadRequestWithoutId)
{
    const auto r = Decode("{not json");

    EXPECT_EQ(r.error, ErrorCode::BadRequest);
    EXPECT_EQ(r.command.id, Command::kNoId);
}

TEST(CodecV1DecodeTest, NonObjectIsBadRequest)
{
    EXPECT_EQ(Decode("[1,2]").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode("42").error, ErrorCode::BadRequest);
}

TEST(CodecV1DecodeTest, MissingOrInvalidIdIsBadRequest)
{
    EXPECT_EQ(Decode(R"({"cmd":"state.get"})").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode(R"({"cmd":"state.get","id":"x"})").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode(R"({"cmd":"state.get","id":-5})").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode(R"({"cmd":"state.get","id":1.5})").error, ErrorCode::BadRequest);
}

TEST(CodecV1DecodeTest, MissingCmdKeepsTheIdForTheErrorResponse)
{
    const auto r = Decode(R"({"id":9})");

    EXPECT_EQ(r.error, ErrorCode::BadRequest);
    EXPECT_EQ(r.command.id, 9);
}

TEST(CodecV1DecodeTest, UnknownCommandKeepsTheId)
{
    const auto r = Decode(R"({"cmd":"fly.away","id":4})");

    EXPECT_EQ(r.error, ErrorCode::UnknownCommand);
    EXPECT_EQ(r.command.id, 4);
}

TEST(CodecV1DecodeTest, PumpSetNeedsABooleanOn)
{
    EXPECT_EQ(Decode(R"({"cmd":"pump.set","id":1})").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode(R"({"cmd":"pump.set","id":1,"data":{}})").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode(R"({"cmd":"pump.set","id":1,"data":{"on":1}})").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode(R"({"cmd":"pump.set","id":1,"data":{"on":"yes"}})").error, ErrorCode::BadRequest);
}

TEST(CodecV1DecodeTest, SettingsSetRejectsBadShapes)
{
    EXPECT_EQ(Decode(R"({"cmd":"settings.set","id":1})").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode(R"({"cmd":"settings.set","id":1,"data":{}})").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode(R"({"cmd":"settings.set","id":1,"data":[1]})").error, ErrorCode::BadRequest);
    EXPECT_EQ(Decode(R"({"cmd":"settings.set","id":1,"data":{"minLevelCm":1.5}})").error, ErrorCode::InvalidValue);
    EXPECT_EQ(Decode(R"({"cmd":"settings.set","id":1,"data":{"minLevelCm":"50"}})").error, ErrorCode::InvalidValue);
}

TEST(CodecV1DecodeTest, SettingsSetRejectsOverlongNamesAndTooManyValues)
{
    const std::string longName(40, 'a');
    EXPECT_EQ(Decode(R"({"cmd":"settings.set","id":1,"data":{")" + longName + R"(":1}})").error,
              ErrorCode::UnknownField);

    std::string many = R"({"cmd":"settings.set","id":1,"data":{)";
    for (int i = 0; i < 17; ++i) many += (i ? "," : "") + std::string("\"f") + std::to_string(i) + "\":1";
    many += "}}";
    EXPECT_EQ(Decode(many).error, ErrorCode::BadRequest);
}

TEST(CodecV1DecodeTest, OversizedMessageIsRejected)
{
    const std::string big = R"({"cmd":"state.get","id":1,"pad":")" + std::string(kMaxMessageBytes, 'x') + "\"}";

    EXPECT_EQ(Decode(big).error, ErrorCode::BadRequest);
}

TEST(CodecV1DecodeTest, NullAndEmptyInputAreRejected)
{
    EXPECT_EQ(DecodeCommand(nullptr, 5).error, ErrorCode::BadRequest);
    EXPECT_EQ(DecodeCommand("", 0).error, ErrorCode::BadRequest);
}

// ---------------------------------------------------------------- encoding

TEST(CodecV1EncodeTest, OkResponseIsCompactAndHasNoVersionField)
{
    EXPECT_EQ(EncodeOk(7), R"({"id":7,"ok":true})");
}

TEST(CodecV1EncodeTest, ErrorResponse)
{
    EXPECT_EQ(EncodeError(7, ErrorCode::PumpCooldown), R"({"id":7,"ok":false,"err":"PUMP_COOLDOWN"})");
}

TEST(CodecV1EncodeTest, ErrorResponseWithoutIdUsesNull)
{
    EXPECT_EQ(EncodeError(Command::kNoId, ErrorCode::BadRequest), R"({"id":null,"ok":false,"err":"BAD_REQUEST"})");
}

TEST(CodecV1EncodeTest, EveryErrorCodeHasAUniqueName)
{
    const ErrorCode codes[] = {ErrorCode::BadRequest, ErrorCode::UnknownCommand, ErrorCode::UnknownField,
                               ErrorCode::InvalidValue, ErrorCode::PumpCooldown, ErrorCode::PumpTooSoon,
                               ErrorCode::HighLevel, ErrorCode::PumpFault, ErrorCode::Internal};
    for (size_t i = 0; i < std::size(codes); ++i)
    {
        EXPECT_STRNE(ErrorCodeName(codes[i]), "");
        for (size_t j = i + 1; j < std::size(codes); ++j) EXPECT_STRNE(ErrorCodeName(codes[i]), ErrorCodeName(codes[j]));
    }
}

TEST(CodecV1EncodeTest, PumpStateEventStartsWithEventKeyAndCarriesTheState)
{
    PumpState pump;
    pump.isOn = true;
    pump.timeStampOn = 1767000000;

    const std::string json = EncodePumpStateEvent(pump);

    EXPECT_EQ(json.rfind(R"({"event":"pump.state")", 0), 0u) << json;
    const auto doc = Parse(json);
    EXPECT_STREQ(doc["data"]["mode"], "running");
    EXPECT_TRUE(doc["data"]["on"].as<bool>());
    EXPECT_FALSE(doc["data"]["cooldown"].as<bool>());
    EXPECT_STREQ(doc["data"]["fault"], "none");
    EXPECT_EQ(doc["data"]["onAt"].as<long long>(), 1767000000LL);
    EXPECT_TRUE(doc["v"].isNull());
}

TEST(CodecV1EncodeTest, FaultAndCooldownAreReported)
{
    PumpState pump;
    pump.isOnCooldown = true;
    pump.fault = PumpFault::NoTime;

    const auto doc = Parse(EncodePumpStateEvent(pump));

    EXPECT_STREQ(doc["data"]["mode"], "fault");
    EXPECT_TRUE(doc["data"]["cooldown"].as<bool>());
    EXPECT_STREQ(doc["data"]["fault"], "noTime");
}

TEST(CodecV1EncodeTest, TankLevelEventRoundsToTenthOfCm)
{
    const auto doc = Parse(EncodeTankLevelEvent(true, 45.23f));

    EXPECT_STREQ(doc["event"], "tank.level");
    EXPECT_NEAR(doc["data"]["cm"].as<float>(), 45.2f, 0.001f);
}

TEST(CodecV1EncodeTest, UnknownLevelIsNull)
{
    EXPECT_EQ(EncodeTankLevelEvent(false, 0.0f), R"({"event":"tank.level","data":{"cm":null}})");
}

TEST(CodecV1EncodeTest, StateResponseHasPumpAndLevel)
{
    const std::string json = EncodeStateResponse(5, PumpState{}, true, 80.0f);

    EXPECT_EQ(json.rfind(R"({"id":5,"ok":true,"data":)", 0), 0u) << json;
    const auto doc = Parse(json);
    EXPECT_STREQ(doc["data"]["pump"]["mode"], "idle");
    EXPECT_NEAR(doc["data"]["level"]["cm"].as<float>(), 80.0f, 0.001f);
}

TEST(CodecV1EncodeTest, SettingsResponseAndEventContainEveryField)
{
    const Settings settings;

    const auto response = Parse(EncodeSettingsResponse(2, settings));
    const auto event = Parse(EncodeSettingsChangedEvent(settings));

    EXPECT_STREQ(event["event"], "settings.changed");
    for (size_t i = 0; i < SettingFieldCount(); ++i)
    {
        const auto& field = SettingFieldAt(i);
        EXPECT_EQ(response["data"][field.name].as<int>(), settings.*(field.member)) << field.name;
        EXPECT_EQ(event["data"][field.name].as<int>(), settings.*(field.member)) << field.name;
    }
}

// ---------------------------------------------------------------- round trip

TEST(CodecV1RoundTripTest, SettingsFromTheEventCanBeSentBackAsASettingsSet)
{
    Settings settings;
    settings.pumpMaxTimeOnM = 33;
    const auto event = Parse(EncodeSettingsChangedEvent(settings));

    std::string payload;
    serializeJson(event["data"], payload);
    const auto r = Decode(R"({"cmd":"settings.set","id":1,"data":)" + payload + "}");

    ASSERT_EQ(r.error, ErrorCode::None);
    Settings applied;
    std::vector<SettingUpdate> updates;
    for (size_t i = 0; i < r.command.valueCount; ++i)
        updates.push_back({r.command.values[i].name, r.command.values[i].value});
    EXPECT_EQ(SetSettings(applied, updates.data(), updates.size()), SettingResult::Ok);
    EXPECT_EQ(applied.pumpMaxTimeOnM, 33);
}

TEST(CodecV1RoundTripTest, ResultsMapToErrorCodes)
{
    EXPECT_EQ(ToErrorCode(PumpResult::Ok), ErrorCode::None);
    EXPECT_EQ(ToErrorCode(PumpResult::Cooldown), ErrorCode::PumpCooldown);
    EXPECT_EQ(ToErrorCode(PumpResult::TooSoon), ErrorCode::PumpTooSoon);
    EXPECT_EQ(ToErrorCode(PumpResult::HighLevel), ErrorCode::HighLevel);
    EXPECT_EQ(ToErrorCode(PumpResult::Fault), ErrorCode::PumpFault);
    EXPECT_EQ(ToErrorCode(SettingResult::Ok), ErrorCode::None);
    EXPECT_EQ(ToErrorCode(SettingResult::UnknownField), ErrorCode::UnknownField);
    EXPECT_EQ(ToErrorCode(SettingResult::OutOfRange), ErrorCode::InvalidValue);
    EXPECT_EQ(ToErrorCode(SettingResult::Inconsistent), ErrorCode::InvalidValue);
}

#include "../support/Main.h"
