#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <map>
#include <string>

#include "core/ports/IStore.h"

namespace CE::Adapters
{
    /// IStore on SPIFFS: all values live in memory and in one small JSON file (/store.json).
    /// SetI64 only stages a value in memory; Commit() writes the file once, and only if something changed.
    /// (Writing the file takes a while on SPIFFS, so it must not happen once per key.)
    /// Safe to call from several tasks.
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
        bool Commit() override;

    private:
        bool Flush();  // caller holds the lock

        static constexpr const char* kFile = "/store.json";
        static constexpr const char* kTempFile = "/store.tmp";

        mutable SemaphoreHandle_t _mutex;
        std::map<std::string, int64_t> _values;     // what readers see (includes staged values)
        std::map<std::string, int64_t> _committed;  // what is in the file
    };
}
