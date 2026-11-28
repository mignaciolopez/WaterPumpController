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

        /// Writes an integer. Returns false if the value could not be persisted.
        virtual bool SetI64(const char* key, int64_t value) = 0;
    };
}
