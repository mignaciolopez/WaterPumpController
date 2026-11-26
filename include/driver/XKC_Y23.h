//
// Created by lmartinignacio@gmail.com on 4/10/2026.
//

#pragma once
#include <Arduino.h>

namespace CE::Driver
{
    class XKC_Y23
    {
    public:
        explicit XKC_Y23(byte pullupPin);
        XKC_Y23(byte pullupPin, const char* debugName);

        [[nodiscard]] bool Setup() const;
        [[nodiscard]] bool IsDetectingWater() const;

    private:
        const char* TAG = "Driver-XKC_Y23";
        byte _pullupPin;
    };
} // Drivers
