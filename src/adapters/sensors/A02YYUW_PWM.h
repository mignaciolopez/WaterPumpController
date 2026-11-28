#pragma once

#include <Arduino.h>

#include "core/ports/IDistanceSensor.h"

namespace CE::Adapters
{
    /// A02YYUW ultrasonic sensor in pulse (PWM) mode: trigger pulse out, echo pulse width in.
    /// Read() blocks while waiting for the echo (pulseIn default timeout of 1 s when nothing answers).
    class A02YYUW_PWM final : public Core::IDistanceSensor
    {
    public:
        /// `rxPin` is the ESP32 pin that receives the echo (sensor TX -> ESP RX).
        /// `txPin` is the ESP32 pin that sends the trigger (ESP TX -> sensor RX).
        A02YYUW_PWM(byte rxPin, byte txPin);

        [[nodiscard]] bool Begin();

        bool Read(float& distanceCm) override;

        static constexpr float kMinDistanceCm = 3.0f;
        static constexpr float kMaxDistanceCm = 450.0f;

    private:
        // Speed of sound in air at 20 C, in cm per microsecond.
        static constexpr float kSpeedCmPerUs = (331.3f + 0.606f * 20.0f) / 10000.0f;

        byte _rxPin;
        byte _txPin;
    };
}
