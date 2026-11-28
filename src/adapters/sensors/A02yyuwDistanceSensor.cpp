#include "adapters/sensors/A02yyuwDistanceSensor.h"

namespace CE::Adapters
{
    A02yyuwDistanceSensor::A02yyuwDistanceSensor(const byte echoPin, const byte triggerPin)
        : _echoPin(echoPin), _triggerPin(triggerPin)
    {
    }

    void A02yyuwDistanceSensor::Begin()
    {
        pinMode(_echoPin, INPUT);
        pinMode(_triggerPin, OUTPUT);
        digitalWrite(_triggerPin, LOW);
    }

    bool A02yyuwDistanceSensor::Read(float& distanceCm)
    {
        // Do not add code between the trigger pulse and pulseIn: it would shorten the measured echo.
        digitalWrite(_triggerPin, LOW);
        delayMicroseconds(2);
        digitalWrite(_triggerPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(_triggerPin, LOW);
        const unsigned long durationUs = pulseIn(_echoPin, HIGH);

        if (durationUs == 0) return false;

        const float cm = static_cast<float>(durationUs) * kSpeedCmPerUs / 2.0f;
        if (cm < kMinDistanceCm || cm > kMaxDistanceCm) return false;

        distanceCm = cm;
        return true;
    }
}
