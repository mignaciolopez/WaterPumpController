#include "adapters/platform/SntpClock.h"

#include <Arduino.h>

#include <ctime>

namespace CE::Adapters
{
    namespace
    {
        constexpr time_t kFirstValidTime = 1577836800;  // 2020-01-01 UTC: anything earlier means "not synced"
    }

    SntpClock::SntpClock(const char* server) : _server(server)
    {
    }

    void SntpClock::Begin()
    {
        configTime(0, 0, _server);  // keep the system clock in UTC; the offset is applied in LocalHour()
    }

    int64_t SntpClock::NowS() const
    {
        return static_cast<int64_t>(time(nullptr));
    }

    bool SntpClock::IsSynced() const
    {
        return time(nullptr) >= kFirstValidTime;
    }

    int SntpClock::LocalHour() const
    {
        const time_t local = time(nullptr) + static_cast<time_t>(_utcOffsetMinutes.load()) * 60;
        tm parts = {};
        gmtime_r(&local, &parts);
        return parts.tm_hour;
    }
}
