#pragma once

#include <cstdint>

namespace CE::App
{
    /// Which distance sensor (and mode) a device uses.
    enum class DistanceSensorType : uint8_t
    {
        A02YYUW_PWM,   // trigger pulse out, echo pulse in
        A02YYUW_UART   // sensor streams frames over a serial port
    };

    /// Everything that differs between devices: pins, sensor choice, credentials and network setup.
    /// One header per device lives in app/profiles/.
    struct Profile
    {
        uint8_t pumpRelayPin;

        uint8_t xkcLowPin;          // XKC_Y23 Low
        uint8_t xkcHighPin;         // XKC_Y23 High

        DistanceSensorType distanceSensor;
        uint8_t distanceRxPin;      // ESP32 RX pin (wired to the A02YYUW TX)
        uint8_t distanceTxPin;      // ESP32 TX pin (wired to the A02YYUW RX)
        uint8_t distanceSerial;     // A02YYUW_UART only: serial port number, 1 or 2 (0 is the console)

        const char* wifiSsid;
        const char* wifiPassword;
        const char* ntpServer;
        uint16_t    httpPort;       // serves the WebSocket (/ws/v1) and the REST API (/api/v1)
    };
}
