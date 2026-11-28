#pragma once

namespace CE::Core
{
    /// A distance sensor (ultrasonic...). Measuring may block for a short time.
    class IDistanceSensor
    {
    public:
        virtual ~IDistanceSensor() = default;

        /// Takes one measurement. Returns false if no valid reading was obtained.
        virtual bool Read(float& distanceCm) = 0;
    };
}
