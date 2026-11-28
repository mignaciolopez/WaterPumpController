#pragma once

namespace CE::Core
{
    enum class LogLevel
    {
        Debug,
        Info,
        Warn,
        Error
    };

    /// Logging sink. `message` is already formatted.
    class ILog
    {
    public:
        virtual ~ILog() = default;

        virtual void Log(LogLevel level, const char* tag, const char* message) = 0;
    };
}
