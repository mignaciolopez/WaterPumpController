#pragma once

namespace CE::Core
{
    /// Restarts the device (last resort of the watchdog).
    class IRestarter
    {
    public:
        virtual ~IRestarter() = default;

        virtual void Restart() = 0;
    };
}
