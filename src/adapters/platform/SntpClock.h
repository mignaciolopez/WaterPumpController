#pragma once

#include "core/ports/IClock.h"

namespace CE::Adapters
{
    /// IClock backed by SNTP. Begin() does not block: IsSynced() turns true once the time arrives
    /// (the pump stays in Fault/NoTime until then).
    class SntpClock final : public Core::IClock
    {
    public:
        /// `timeZone` is a POSIX TZ string, e.g. "ART3".
        SntpClock(const char* timeZone, const char* server);

        void Begin();

        int64_t NowS() const override;
        bool IsSynced() const override;
        int LocalHour() const override;

    private:
        const char* _timeZone;
        const char* _server;
    };
}
