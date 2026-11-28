#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <map>
#include <string>

#include "core/ports/IStore.h"

namespace CE::Adapters
{
    /// IStore on SPIFFS: all values live in memory and in one small JSON file (/store.json).
    /// A value is written to flash only when it changes. Safe to call from several tasks.
    class SpiffsStore final : public Core::IStore
    {
    public:
        SpiffsStore();
        ~SpiffsStore() override;

        SpiffsStore(const SpiffsStore&) = delete;
        SpiffsStore& operator=(const SpiffsStore&) = delete;

        /// Mounts SPIFFS (formatting it on first use) and loads the stored values.
        /// A missing or unreadable file is not an error (starts empty). Returns false if the mount fails.
        [[nodiscard]] bool Begin();

        bool GetI64(const char* key, int64_t& value) const override;
        bool SetI64(const char* key, int64_t value) override;

    private:
        bool Flush();  // caller holds the lock

        static constexpr const char* kFile = "/store.json";
        static constexpr const char* kTempFile = "/store.tmp";

        mutable SemaphoreHandle_t _mutex;
        std::map<std::string, int64_t> _values;
    };
}
