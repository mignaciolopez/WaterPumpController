#include "adapters/net/SnapshotCache.h"

#include <utility>

namespace CE::Adapters
{
    namespace
    {
        class Lock
        {
        public:
            explicit Lock(const SemaphoreHandle_t mutex) : _mutex(mutex) { xSemaphoreTake(_mutex, portMAX_DELAY); }
            ~Lock() { xSemaphoreGive(_mutex); }
            Lock(const Lock&) = delete;
            Lock& operator=(const Lock&) = delete;

        private:
            SemaphoreHandle_t _mutex;
        };
    }

    SnapshotCache::SnapshotCache() : _mutex(xSemaphoreCreateMutex())
    {
    }

    SnapshotCache::~SnapshotCache()
    {
        vSemaphoreDelete(_mutex);
    }

    void SnapshotCache::Set(std::string state, std::string settings)
    {
        Lock lock(_mutex);
        _state = std::move(state);
        _settings = std::move(settings);
    }

    std::string SnapshotCache::State() const
    {
        Lock lock(_mutex);
        return _state;
    }

    std::string SnapshotCache::Settings() const
    {
        Lock lock(_mutex);
        return _settings;
    }
}
