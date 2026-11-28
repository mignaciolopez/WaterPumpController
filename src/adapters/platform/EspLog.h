#pragma once

#include "core/ports/ILog.h"

namespace CE::Adapters
{
    /// ILog on top of the ESP-IDF / Arduino logging macros.
    class EspLog final : public Core::ILog
    {
    public:
        void Log(Core::LogLevel level, const char* tag, const char* message) override;
    };
}
