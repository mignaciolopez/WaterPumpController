//
// Created by lmartinignacio@gmail.com on 2/27/2026.
//

#include "domain/Global.hpp"
#include <driver/HC-SR04.h>
#include <os/Settings.h>

namespace CE::Driver
{
    HC_SR04::HC_SR04(const byte rxPin, const byte txPin) : _rxPin(rxPin), _txPin(txPin)
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "_rxPin %d", _rxPin);
        ESP_LOGD(TAG, "_txPin %d", _txPin);
    }

    HC_SR04::HC_SR04(const byte rxPin, const byte txPin, const char* debugName) : TAG(debugName), _rxPin(rxPin), _txPin(txPin)
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "_rxPin %d", _rxPin);
        ESP_LOGD(TAG, "_txPin %d", _txPin);
    }

    bool HC_SR04::Setup() const
    {
        ESP_LOGV(TAG, "Setup");

        pinMode(_rxPin, INPUT);
        pinMode(_txPin, OUTPUT);

        ESP_LOGD(TAG, "_rxPin %d", _rxPin);
        ESP_LOGD(TAG, "_txPin %d", _txPin);

        return true;
    }

    bool HC_SR04::TryRead()
    {
        //ESP_LOGV(TAG, "TryRead");

        // Timeout prevents blocking forever
        const auto timeout = static_cast<unsigned long>(static_cast<float>(OS::Settings::Get().heightCm) * 2.25f / Domain::g_speed_cm_per_us);
        //ESP_LOGD(TAG, "timeout_us=%u", timeout);

        unsigned long duration_us = 0;

        { // Do not add code while waiting for echo on pulseIn function call
            // Trigger pulse
            digitalWrite(_txPin, LOW);
            delayMicroseconds(2);
            digitalWrite(_txPin, HIGH);
            delayMicroseconds(10);
            digitalWrite(_txPin, LOW);
            duration_us = pulseIn(_rxPin, HIGH, timeout);
        }

        if (duration_us == 0)
        {
            return false;
        }

        sample.distanceCm = static_cast<float>(duration_us) * Domain::g_speed_cm_per_us / 2.0f;
        return true;
    }

}   // namespace CE::Drivers