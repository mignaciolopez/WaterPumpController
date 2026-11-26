//
// Created by lmartinignacio@gmail.com on 4/10/2026.
//

#pragma once
#include <Arduino.h>

namespace CE::Driver
{
    class Relay
    {
    public:
        explicit Relay(byte switchPin);
        Relay(byte pullupPin, const char* debugName);

        [[nodiscard]] bool Setup();
        void Switch(bool state);
        void SwitchOn();
        void SwitchOff();
        [[nodiscard]] bool IsOn() const;

    private:
        const char* TAG = "Driver-Relay";
        byte _switchPin;
        bool _state;
    };
} // Drivers
