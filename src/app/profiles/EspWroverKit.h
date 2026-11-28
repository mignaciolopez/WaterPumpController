#pragma once

#include "app/Profile.h"
#include "secrets.h"  // WIFI_SSID, WIFI_PASS (copy include/secrets.h.template to include/secrets.h)

namespace CE::App
{
    // ESP32 WROVER kit.
    //
    // The old firmware swapped the two float-switch pins in the pump constructor, and the pump was
    // disabled in App::Setup(), so this wiring was never really exercised. The names below follow the
    // old Pins.hpp: low sensor on GPIO 26, high sensor on GPIO 27.
    // If your sensors are wired the other way around the pump will report a sensor fault and stay off.
    inline constexpr Profile kEspWroverKit{
        /* pumpRelayPin       */ 25,
        /* floatLowPin        */ 26,
        /* floatHighPin       */ 27,
        /* distanceEchoPin    */ 18,  // RX <- remote TX
        /* distanceTriggerPin */ 19,  // TX -> remote RX
        /* wifiSsid           */ WIFI_SSID,
        /* wifiPassword       */ WIFI_PASS,
        /* ntpServer          */ "pool.ntp.org",
        /* webSocketPort      */ 80,
    };
}
