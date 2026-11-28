#include "adapters/sensors/XKC_Y23.h"

namespace CE::Adapters
{
    XKC_Y23::XKC_Y23(const byte pin) : _pin(pin)
    {
    }

    void XKC_Y23::Begin()
    {
        pinMode(_pin, INPUT_PULLUP);
    }

    bool XKC_Y23::IsDetectingWater() const
    {
        return digitalRead(_pin) == LOW;
    }
}
