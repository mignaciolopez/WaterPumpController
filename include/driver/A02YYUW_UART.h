//
// Created by lmart on 4/11/2026.
//

#pragma once
#include <Arduino.h>
#include <domain/Samples.hpp>

namespace CE::Driver
{
    class A02YYUW_UART
    {
    public:
        explicit A02YYUW_UART(byte rxPin, byte txPin, HardwareSerial& serial);
        A02YYUW_UART(byte rxPin, byte txPin, HardwareSerial& serial, const char* debugName);

        [[nodiscard]] bool Setup() const;

        bool TryRead();

    private:

    public:
        Domain::RadarSample sample;

    private:
        const char* TAG = "Driver-A02YYUW-UART";
        const uint16_t BAUD_RATE = 9600;
        const SerialConfig CONFIG = SERIAL_8N1;
        byte _rxPin;
        byte _txPin;
        HardwareSerial& _serial;

        uint8_t _index;
        uint8_t _buffer[4];
    };
}
