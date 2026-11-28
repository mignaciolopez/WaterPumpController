#include "adapters/sensors/XkcFloatSwitch.h"

namespace CE::Adapters
{
    XkcFloatSwitch::XkcFloatSwitch(const byte pin) : _pin(pin)
    {
    }

    void XkcFloatSwitch::Begin()
    {
        pinMode(_pin, INPUT_PULLUP);
    }

    bool XkcFloatSwitch::IsDetectingWater() const
    {
        return digitalRead(_pin) == LOW;
    }
}
