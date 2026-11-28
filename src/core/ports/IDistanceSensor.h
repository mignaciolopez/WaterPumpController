#pragma once

#include <cstdint>

namespace CE::Core
{
    enum class DistanceStatus : uint8_t
    {
        Ok,          // a normal measurement: `cm` is the distance
        TooClose,    // an echo arrived but it is shorter than the sensor's minimum range: `cm` is that minimum
        NoEcho,      // nothing came back (water inside the blind zone, nothing in range, or a dead sensor)
        OutOfRange   // an echo arrived but beyond the sensor's maximum range
    };

    struct DistanceReading
    {
        DistanceStatus status = DistanceStatus::NoEcho;
        float cm = 0.0f;
    };

    /// Sorts a measured distance into Ok / TooClose / OutOfRange for a sensor with the given range.
    /// A distance below the minimum is clamped to the minimum: the object is at least that close.
    inline DistanceReading ClassifyDistance(const float cm, const float minCm, const float maxCm)
    {
        if (cm < minCm) return {DistanceStatus::TooClose, minCm};
        if (cm > maxCm) return {DistanceStatus::OutOfRange, 0.0f};
        return {DistanceStatus::Ok, cm};
    }

    /// A distance sensor (ultrasonic...). Measuring may block for a short time.
    class IDistanceSensor
    {
    public:
        virtual ~IDistanceSensor() = default;

        /// Takes one measurement.
        virtual DistanceReading Read() = 0;

        /// The closest distance the sensor can measure (its blind zone ends here).
        virtual float MinDistanceCm() const = 0;
    };
}
