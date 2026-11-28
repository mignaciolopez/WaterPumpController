#pragma once

#include <cstdint>

namespace CE::Core
{
    /// Small persistent key/value store (survives reboots).
    class IStore
    {
    public:
        virtual ~IStore() = default;

        /// Reads an integer. Returns false if the key does not exist or cannot be read.
        virtual bool GetI64(const char* key, int64_t& value) const = 0;

        /// Stages an integer. It is only guaranteed to survive a reboot after Commit() succeeded.
        /// Returns false if the value cannot be stored at all.
        virtual bool SetI64(const char* key, int64_t value) = 0;

        /// Persists everything staged since the last Commit() in one write (flash writes are slow and
        /// wear the flash, so callers set all their keys first and commit once).
        /// Returns false if it could not be persisted; the staged values are then discarded.
        virtual bool Commit() = 0;
    };
}
