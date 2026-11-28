#pragma once

#include <Arduino.h>

#include "core/ports/IFloatSwitch.h"

namespace CE::Adapters
{
    /// XKC-Y23 contactless level sensor on a GPIO pin (input with pull-up; LOW = water detected).
    class XkcFloatSwitch final : public Core::IFloatSwitch
    {
    public:
        explicit XkcFloatSwitch(byte pin);

        void Begin();

        bool IsDetectingWater() const override;

    private:
        byte _pin;
    };
}
