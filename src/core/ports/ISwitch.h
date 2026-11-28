#pragma once

namespace CE::Core
{
    /// A controllable on/off output (relay, valve...).
    class ISwitch
    {
    public:
        virtual ~ISwitch() = default;

        virtual void Set(bool on) = 0;
        virtual bool IsOn() const = 0;
    };
}
