//
// Created by lmartinignacio@gmail.com on 4/10/2026.
//

#include <driver/Relay.h>

namespace CE::Driver
{
    Relay::Relay(const byte switchPin) : _switchPin(switchPin), _state(false)
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "switchPin %d", switchPin);
    }

    Relay::Relay(const byte switchPin, const char* debugName) : TAG(debugName), _switchPin(switchPin), _state(false)
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "switchPin %d", switchPin);
    }

    bool Relay::Setup()
    {
        ESP_LOGV(TAG, "Setup");

        ESP_LOGD(TAG, "switchPin %d", _switchPin);

        pinMode(_switchPin, OUTPUT);
        digitalWrite(_switchPin, LOW);
        _state = false;

        return true;
    }

    void Relay::Switch(const bool state)
    {
        ESP_LOGD(TAG, "Switch %s", state ? "true" : "false");
        const auto val = state ? HIGH : LOW;
        digitalWrite(_switchPin, val);
        _state = state;
    }

    void Relay::SwitchOn()
    {
        Switch(true);
    }

    void Relay::SwitchOff()
    {
        Switch(false);
    }

    bool Relay::IsOn() const
    {
        return _state;
    }
} // CE