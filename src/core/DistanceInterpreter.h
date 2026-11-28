#pragma once

#include <cstdint>

#include "core/ports/IDistanceSensor.h"

namespace CE::Core
{
    /// How close to its limits a reading must be to count as "at the edge of the sensor's range".
    constexpr float kDistanceEdgeMarginCm = 2.0f;

    /// Makes sense of what an ultrasonic sensor with a limited range reports.
    ///
    /// Such a sensor cannot measure closer than its minimum or farther than its maximum: it simply gives no echo,
    /// so "too close" is never seen directly. What can be seen is the reading just before: water that rises into
    /// the blind zone first reads 6, 5, 4 cm and then goes silent. So:
    ///
    ///  - an echo within `margin` of the minimum is TooClose (its real value is kept), and within `margin` of the
    ///    maximum is OutOfRange; anything else is Ok;
    ///  - silence right after such an edge reading is read as "the water went into the blind zone" (TooClose at the
    ///    minimum, `inferred`) or "beyond the far limit" (OutOfRange at the maximum, `inferred`), and stays so while
    ///    the silence lasts;
    ///  - silence after a normal reading (or with no history) is a failure: NoEcho.
    class DistanceInterpreter
    {
    public:
        DistanceInterpreter(float minCm, float maxCm, float marginCm = kDistanceEdgeMarginCm);

        /// `echo` is false when the sensor gave nothing; `cm` is only used when it did.
        DistanceReading Interpret(bool echo, float cm);

    private:
        enum class Last : uint8_t { Unknown, Middle, NearMin, NearMax };

        float _minCm;
        float _maxCm;
        float _marginCm;
        Last _last = Last::Unknown;
    };
}
