//
// Created by lmartinignacio@gmail.com on 2/27/2026.
//

#pragma once
#include <Arduino.h>

#include "domain/Samples.hpp"

namespace CE::Driver
{
    class A02YYUW_PWM
    {
    public:
        explicit A02YYUW_PWM(byte rxPin, byte txPin);
        A02YYUW_PWM(byte rxPin, byte txPin, const char* debugName);

        [[nodiscard]] bool Setup() const;
        bool TryRead();

    public:
        Domain::RadarSample sample;

    private:
        const char* TAG = "Driver-A02YYUW-PWM";
        byte _rxPin;
        byte _txPin;
        static constexpr uint16_t minDistance_cm = 3;
        static constexpr uint16_t maxDistance_cm = 450;
    };

}   // namespace CE::Drivers