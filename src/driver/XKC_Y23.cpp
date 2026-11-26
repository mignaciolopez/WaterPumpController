//
// Created by lmartinignacio@gmail.com on 4/10/2026.
//

#include <driver/XKC_Y23.h>

namespace CE::Driver
{
    XKC_Y23::XKC_Y23(const byte pullupPin) : _pullupPin(pullupPin)
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "pullupPin %d", pullupPin);
    }

    XKC_Y23::XKC_Y23(const byte pullupPin, const char* debugName) : TAG(debugName), _pullupPin(pullupPin)
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "pullupPin %d", _pullupPin);
    }

    bool XKC_Y23::Setup() const
    {
        ESP_LOGV(TAG, "Setup");

        ESP_LOGD(TAG, "pullupPin %d", _pullupPin);
        pinMode(_pullupPin, INPUT_PULLUP);

        return true;
    }

    bool XKC_Y23::IsDetectingWater() const
    {
        const auto waterDetected = digitalRead(_pullupPin) == LOW;
        ESP_LOGD(TAG, "IsDetectingWater: %s", waterDetected ? "true" : "false");
        return waterDetected;
    }
} // CE