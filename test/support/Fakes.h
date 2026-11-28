#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/ports/IClock.h"
#include "core/ports/IFloatSwitch.h"
#include "core/ports/ILog.h"
#include "core/ports/IStore.h"
#include "core/ports/ISwitch.h"

namespace CE::Fakes
{
    class FakeClock : public Core::IClock
    {
    public:
        int64_t now = 1000000;
        bool synced = true;
        int localHour = 12;

        int64_t NowS() const override { return now; }
        bool IsSynced() const override { return synced; }
        int LocalHour() const override { return localHour; }
        void Advance(const int64_t seconds) { now += seconds; }
    };

    class FakeSwitch : public Core::ISwitch
    {
    public:
        bool on = false;
        int setCount = 0;

        void Set(const bool value) override { on = value; ++setCount; }
        bool IsOn() const override { return on; }
    };

    class FakeFloat : public Core::IFloatSwitch
    {
    public:
        bool wet = false;

        bool IsDetectingWater() const override { return wet; }
    };

    class FakeStore : public Core::IStore
    {
    public:
        std::map<std::string, int64_t> values;
        bool failWrites = false;

        bool GetI64(const char* key, int64_t& value) const override
        {
            const auto it = values.find(key);
            if (it == values.end()) return false;
            value = it->second;
            return true;
        }

        bool SetI64(const char* key, const int64_t value) override
        {
            if (failWrites) return false;
            values[key] = value;
            return true;
        }
    };

    class FakeLog : public Core::ILog
    {
    public:
        std::vector<std::string> messages;

        void Log(Core::LogLevel, const char*, const char* message) override { messages.emplace_back(message); }
    };
}
