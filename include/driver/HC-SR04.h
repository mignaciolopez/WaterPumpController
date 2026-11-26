//
// Created by lmartinignacio@gmail.com on 2/27/2026.
//

#pragma once

#include <Arduino.h>
#include "domain/Samples.hpp"

namespace CE::Driver
{
    class HC_SR04
    {
    public:
        explicit HC_SR04(byte rxPin, byte txPin);
        HC_SR04(byte rxPin, byte txPin, const char* debugName);

        [[nodiscard]] bool Setup() const;
        bool TryRead();

    public:
        Domain::RadarSample sample;

    private:
        const char* TAG = "Driver-HC_SR04";
        byte _rxPin; //Echo
        byte _txPin; //Trigger
    };

}   // namespace CE::Drivers