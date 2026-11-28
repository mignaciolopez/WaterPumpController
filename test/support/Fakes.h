#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/ports/IClock.h"
#include "core/ports/IFloatSwitch.h"
#include "core/ports/IMessageSink.h"
#include "core/ports/INetwork.h"
#include "core/ports/IRestarter.h"
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
        bool failWrites = false;     // SetI64 fails
        bool failCommits = false;    // Commit fails
        int commitCount = 0;

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

        bool Commit() override
        {
            ++commitCount;
            return !failWrites && !failCommits;
        }
    };

    class FakeLog : public Core::ILog
    {
    public:
        struct Entry { Core::LogLevel level; std::string tag; std::string message; };

        std::vector<std::string> messages;   // messages only
        std::vector<Entry> entries;          // level + tag + message

        void Log(const Core::LogLevel level, const char* tag, const char* message) override
        {
            messages.emplace_back(message);
            entries.push_back({level, tag, message});
        }
    };

    class FakeNetwork : public Core::INetwork
    {
    public:
        bool connected = true;
        bool pingOk = true;
        int reconnectCount = 0;

        bool IsConnected() const override { return connected; }
        bool PingGateway() override { return pingOk; }
        void Reconnect() override { ++reconnectCount; }
    };

    class FakeRestarter : public Core::IRestarter
    {
    public:
        int restartCount = 0;

        void Restart() override { ++restartCount; }
    };

    class FakeSink : public Core::IMessageSink
    {
    public:
        struct Sent { uint32_t clientId; std::string message; };

        std::vector<Sent> sent;                  // responses
        std::vector<std::string> broadcasts;     // events

        void Send(const uint32_t clientId, const std::string& message) override { sent.push_back({clientId, message}); }
        void Broadcast(const std::string& message) override { broadcasts.push_back(message); }
    };
}
