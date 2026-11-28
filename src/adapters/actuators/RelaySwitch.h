#pragma once

#include <Arduino.h>

#include "core/ports/ISwitch.h"

namespace CE::Adapters
{
    /// Relay on a GPIO pin, active HIGH.
    class RelaySwitch final : public Core::ISwitch
    {
    public:
        explicit RelaySwitch(byte pin);

        /// Configures the pin and makes sure the relay is off.
        void Begin();

        void Set(bool on) override;
        bool IsOn() const override { return _on; }

    private:
        byte _pin;
        bool _on = false;
    };
}
