#pragma once

#include <Arduino.h>

#include "core/ports/IFloatSwitch.h"

namespace CE::Adapters
{
    /// XKC_Y23 contactless level sensor on a GPIO pin (input with pull-up; LOW = water detected).
    class XKC_Y23 final : public Core::IFloatSwitch
    {
    public:
        explicit XKC_Y23(byte pin);

        void Begin();

        bool IsDetectingWater() const override;

    private:
        byte _pin;
    };
}
