#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <string>

namespace CE::Adapters
{
    /// The latest state and settings as ready-made JSON. The control task writes it whenever something
    /// changes; the REST handlers read it, so a GET never has to touch the pump logic.
    class SnapshotCache
    {
    public:
        SnapshotCache();
        ~SnapshotCache();

        SnapshotCache(const SnapshotCache&) = delete;
        SnapshotCache& operator=(const SnapshotCache&) = delete;

        void Set(std::string state, std::string settings);

        std::string State() const;
        std::string Settings() const;

    private:
        mutable SemaphoreHandle_t _mutex;
        std::string _state = "{}";
        std::string _settings = "{}";
    };
}
