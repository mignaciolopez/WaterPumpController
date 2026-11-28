#pragma once

#include <Arduino.h>

#include "core/ports/IDistanceSensor.h"

namespace CE::Adapters
{
    /// A02YYUW ultrasonic sensor in pulse (PWM) mode: trigger pulse out, echo pulse width in.
    /// Read() blocks while waiting for the echo (pulseIn default timeout of 1 s when nothing answers).
    class A02yyuwDistanceSensor final : public Core::IDistanceSensor
    {
    public:
        /// `echoPin` receives the pulse, `triggerPin` sends the trigger.
        A02yyuwDistanceSensor(byte echoPin, byte triggerPin);

        void Begin();

        bool Read(float& distanceCm) override;

        static constexpr float kMinDistanceCm = 3.0f;
        static constexpr float kMaxDistanceCm = 450.0f;

    private:
        // Speed of sound in air at 20 C, in cm per microsecond.
        static constexpr float kSpeedCmPerUs = (331.3f + 0.606f * 20.0f) / 10000.0f;

        byte _echoPin;
        byte _triggerPin;
    };
}
