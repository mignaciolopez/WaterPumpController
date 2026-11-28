#include "adapters/platform/SntpClock.h"

#include <Arduino.h>

#include <ctime>

namespace CE::Adapters
{
    namespace
    {
        constexpr time_t kFirstValidTime = 1577836800;  // 2020-01-01 UTC: anything earlier means "not synced"
    }

    SntpClock::SntpClock(const char* timeZone, const char* server) : _timeZone(timeZone), _server(server)
    {
    }

    void SntpClock::Begin()
    {
        configTzTime(_timeZone, _server);
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
        const time_t now = time(nullptr);
        tm local = {};
        localtime_r(&now, &local);
        return local.tm_hour;
    }
}
