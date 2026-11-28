#include "adapters/sensors/A02YYUW_PWM.h"

namespace CE::Adapters
{
    A02YYUW_PWM::A02YYUW_PWM(const byte rxPin, const byte txPin) : _rxPin(rxPin), _txPin(txPin)
    {
    }

    bool A02YYUW_PWM::Begin()
    {
        pinMode(_rxPin, INPUT);
        pinMode(_txPin, OUTPUT);
        digitalWrite(_txPin, LOW);
        return true;
    }

    bool A02YYUW_PWM::Read(float& distanceCm)
    {
        // Do not add code between the trigger pulse and pulseIn: it would shorten the measured echo.
        digitalWrite(_txPin, LOW);
        delayMicroseconds(2);
        digitalWrite(_txPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(_txPin, LOW);
        const unsigned long durationUs = pulseIn(_rxPin, HIGH);

        if (durationUs == 0) return false;

        const float cm = static_cast<float>(durationUs) * kSpeedCmPerUs / 2.0f;
        if (cm < kMinDistanceCm || cm > kMaxDistanceCm) return false;

        distanceCm = cm;
        return true;
    }
}
