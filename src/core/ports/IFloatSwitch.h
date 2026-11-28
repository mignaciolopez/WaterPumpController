#pragma once

namespace CE::Core
{
    /// A binary water-presence sensor (float switch, XKC-Y23...).
    class IFloatSwitch
    {
    public:
        virtual ~IFloatSwitch() = default;

        /// True when the sensor is submerged / detects water.
        virtual bool IsDetectingWater() const = 0;
    };
}
