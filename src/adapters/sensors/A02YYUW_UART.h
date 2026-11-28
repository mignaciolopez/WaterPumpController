#pragma once

#include <Arduino.h>

#include "adapters/sensors/A02YYUW_Frame.h"
#include "core/DistanceInterpreter.h"
#include "core/ports/IDistanceSensor.h"

namespace CE::Adapters
{
    /// A02YYUW ultrasonic sensor in UART mode (9600 baud, 8N1). The sensor streams frames by itself.
    /// Read() does not block: it consumes the bytes received so far and returns the latest valid frame.
    class A02YYUW_UART final : public Core::IDistanceSensor
    {
    public:
        /// `rxPin` is the ESP32 pin that receives data (sensor TX -> ESP RX).
        /// `txPin` is the ESP32 pin that sends data (ESP TX -> sensor RX).
        A02YYUW_UART(byte rxPin, byte txPin, HardwareSerial& serial);

        /// Opens the serial port. Returns false if it cannot be opened.
        [[nodiscard]] bool Begin();

        /// NoEcho when no complete valid frame arrived since the last call.
        Core::DistanceReading Read() override;
        float MinDistanceCm() const override { return kMinDistanceCm; }
        float MaxDistanceCm() const override { return kMaxDistanceCm; }

        static constexpr float kMinDistanceCm = 3.0f;
        static constexpr float kMaxDistanceCm = 450.0f;

    private:
        static constexpr uint32_t kBaudRate = 9600;

        byte _rxPin;
        byte _txPin;
        HardwareSerial& _serial;
        A02YYUW_FrameParser _parser;
        Core::DistanceInterpreter _interpreter{kMinDistanceCm, kMaxDistanceCm};
    };
}
