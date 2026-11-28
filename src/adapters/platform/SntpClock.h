#pragma once

#include <atomic>

#include "core/ports/IClock.h"

namespace CE::Adapters
{
    /// IClock backed by SNTP. Begin() does not block: IsSynced() turns true once the time arrives
    /// (the pump stays in Fault/NoTime until then).
    class SntpClock final : public Core::IClock
    {
    public:
        explicit SntpClock(const char* server);

        void Begin();

        /// Local time = UTC + offset (no daylight saving). Safe to call from any task.
        void SetUtcOffsetMinutes(int minutes) { _utcOffsetMinutes = minutes; }

        int64_t NowS() const override;
        bool IsSynced() const override;
        int LocalHour() const override;

    private:
        const char* _server;
        std::atomic<int> _utcOffsetMinutes{0};
    };
}
