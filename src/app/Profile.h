#pragma once

#include <cstdint>

namespace CE::App
{
    /// Everything that differs between devices: pins, credentials and the network setup.
    /// One header per device lives in app/profiles/.
    struct Profile
    {
        uint8_t pumpRelayPin;
        uint8_t floatLowPin;        // low level sensor (XKC-Y23)
        uint8_t floatHighPin;       // high level sensor (XKC-Y23)
        uint8_t distanceEchoPin;    // ultrasonic echo (input)
        uint8_t distanceTriggerPin; // ultrasonic trigger (output)

        const char* wifiSsid;
        const char* wifiPassword;
        const char* ntpServer;
        uint16_t    webSocketPort;
    };
}
