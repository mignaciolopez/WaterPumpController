#pragma once

#include <cstdint>

namespace CE::Core
{
    enum class DistanceStatus : uint8_t
    {
        Ok,          // a normal measurement: `cm` is the distance
        TooClose,    // at the near limit of the sensor's range (see DistanceInterpreter): `cm` is the distance, >= minimum
        NoEcho,      // nothing came back and nothing explains it: a failure (or a dead sensor)
        OutOfRange   // at the far limit of the sensor's range
    };

    struct DistanceReading
    {
        DistanceStatus status = DistanceStatus::NoEcho;
        float cm = 0.0f;

        /// The sensor said nothing; `status` and `cm` were inferred from the last reading it did give
        /// (water that disappears into the blind zone, for example).
        bool inferred = false;
    };

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
