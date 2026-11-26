//
// Created by lmartinignacio@gmail.com on 2/27/2026.
//

#include "domain/Global.hpp"
#include <driver/A02YYUW_PWM.h>
#include <os/Settings.h>

namespace CE::Driver
{
    A02YYUW_PWM::A02YYUW_PWM(const byte rxPin, const byte txPin) : _rxPin(rxPin), _txPin(txPin)
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "_rxPin %d", _rxPin);
        ESP_LOGD(TAG, "_txPin %d", _txPin);
    }

    A02YYUW_PWM::A02YYUW_PWM(const byte rxPin, const byte txPin, const char* debugName) : TAG(debugName), _rxPin(rxPin), _txPin(txPin)
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "_rxPin %d", _rxPin);
        ESP_LOGD(TAG, "_txPin %d", _txPin);
    }

    bool A02YYUW_PWM::Setup() const
    {
        ESP_LOGV(TAG, "Setup");

        pinMode(_rxPin, INPUT);
        pinMode(_txPin, OUTPUT);

        ESP_LOGD(TAG, "_rxPin %d", _rxPin);
        ESP_LOGD(TAG, "_txPin %d", _txPin);

        return true;
    }

    bool A02YYUW_PWM::TryRead()
    {
        ESP_LOGV(TAG, "TryRead Start");

        unsigned long duration_us = 0;

        { // Do not add code while waiting for echo on pulseIn function call
            ESP_LOGV(TAG, "Triggering Pulse.");
            digitalWrite(_txPin, LOW);
            delayMicroseconds(2);
            digitalWrite(_txPin, HIGH);
            delayMicroseconds(10);
            digitalWrite(_txPin, LOW);
            duration_us = pulseIn(_rxPin, HIGH);
            ESP_LOGV(TAG, "Triggering Pulse Done.");
            ESP_LOGD(TAG, "duration: '%u' us.", duration_us);
        }

        if (duration_us <= 0)
        {
            ESP_LOGV(TAG, "TryRead End.");
            return false;
        }

        sample.distanceCm = static_cast<float>(duration_us) * Domain::g_speed_cm_per_us / 2.0f;

        ESP_LOGV(TAG, "TryRead End.");
        return true;
    }

}   // namespace CE::Drivers