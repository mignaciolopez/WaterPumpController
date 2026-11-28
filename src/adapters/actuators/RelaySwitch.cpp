#include "adapters/actuators/RelaySwitch.h"

namespace CE::Adapters
{
    RelaySwitch::RelaySwitch(const byte pin) : _pin(pin)
    {
    }

    void RelaySwitch::Begin()
    {
        pinMode(_pin, OUTPUT);
        Set(false);
    }

    void RelaySwitch::Set(const bool on)
    {
        digitalWrite(_pin, on ? HIGH : LOW);
        _on = on;
    }
}
