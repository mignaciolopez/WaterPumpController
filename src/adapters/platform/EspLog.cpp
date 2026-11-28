#include "adapters/platform/EspLog.h"

#include <Arduino.h>

namespace CE::Adapters
{
    void EspLog::Log(const Core::LogLevel level, const char* tag, const char* message)
    {
        switch (level)
        {
            case Core::LogLevel::Debug: ESP_LOGD(tag, "%s", message); break;
            case Core::LogLevel::Info:  ESP_LOGI(tag, "%s", message); break;
            case Core::LogLevel::Warn:  ESP_LOGW(tag, "%s", message); break;
            case Core::LogLevel::Error: ESP_LOGE(tag, "%s", message); break;
        }
    }
}
