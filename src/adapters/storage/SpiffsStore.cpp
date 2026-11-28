#include "adapters/storage/SpiffsStore.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

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

    SpiffsStore::SpiffsStore() : _mutex(xSemaphoreCreateMutex())
    {
    }

    SpiffsStore::~SpiffsStore()
    {
        vSemaphoreDelete(_mutex);
    }

    bool SpiffsStore::Begin()
    {
        if (!SPIFFS.begin(true)) return false;

        Lock lock(_mutex);
        _values.clear();
        _committed.clear();

        File file = SPIFFS.open(kFile, "r");
        if (!file) return true;  // first boot

        JsonDocument doc;
        const DeserializationError error = deserializeJson(doc, file);
        file.close();
        if (error) return true;  // corrupt file: start empty, it is rewritten on the next change

        for (JsonPair pair : doc.as<JsonObject>())
        {
            if (pair.value().is<long long>()) _values[pair.key().c_str()] = pair.value().as<long long>();
        }
        _committed = _values;
        return true;
    }

    bool SpiffsStore::GetI64(const char* key, int64_t& value) const
    {
        Lock lock(_mutex);

        const auto it = _values.find(key);
        if (it == _values.end()) return false;

        value = it->second;
        return true;
    }

    bool SpiffsStore::SetI64(const char* key, const int64_t value)
    {
        Lock lock(_mutex);

        _values[key] = value;  // staged; Commit() writes it
        return true;
    }

    bool SpiffsStore::Commit()
    {
        Lock lock(_mutex);

        if (_values == _committed) return true;  // nothing changed: no flash write

        if (Flush())
        {
            _committed = _values;
            return true;
        }

        _values = _committed;  // could not be saved: drop the staged values
        return false;
    }

    bool SpiffsStore::Flush()
    {
        JsonDocument doc;
        for (const auto& [key, value] : _values) doc[key.c_str()] = value;

        // Write a temp file first so a power loss cannot leave a half-written store.
        File file = SPIFFS.open(kTempFile, "w");
        if (!file) return false;

        const size_t written = serializeJson(doc, file);
        file.close();
        if (written == 0) return false;

        SPIFFS.remove(kFile);
        return SPIFFS.rename(kTempFile, kFile);
    }
}
