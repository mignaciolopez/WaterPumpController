#pragma once

#include <cstdint>

namespace CE::Core
{
    /// Wall-clock time source.
    class IClock
    {
    public:
        virtual ~IClock() = default;

        /// Seconds since the Unix epoch (UTC). Only meaningful when IsSynced() is true.
        virtual int64_t NowS() const = 0;

        /// True once the clock holds a trustworthy time (e.g. after NTP sync).
        virtual bool IsSynced() const = 0;
    };
}
