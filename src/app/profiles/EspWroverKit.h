#pragma once

#include "app/Profile.h"
#include "secrets.h"  // WIFI_SSID, WIFI_PASS (copy include/secrets.h.template to include/secrets.h)

namespace CE::App
{
    // ESP32 WROVER kit.
    //
    // The old firmware swapped the two XKC_Y23 pins in the pump constructor, and the pump was
    // disabled in App::Setup(), so this wiring was never really exercised. The pins below follow the
    // names in the old Pins.hpp. If your sensors are wired the other way around, the pump will report
    // a sensor fault and stay off.
    inline constexpr Profile kEspWroverKit{
        /* pumpRelayPin   */ 25,
        /* xkcLowPin      */ 26,  // XKC_Y23 Low
        /* xkcHighPin     */ 27,  // XKC_Y23 High

        /* distanceSensor */ DistanceSensorType::A02YYUW_PWM,
        /* distanceRxPin  */ 19,  // ESP RX <- A02YYUW TX
        /* distanceTxPin  */ 18,  // ESP TX -> A02YYUW RX
        /* distanceSerial */ 2,   // used by A02YYUW_UART only

        /* weatherSensor  */ WeatherSensorType::Dht11,
        /* weatherPin     */ 21,  // DHT11 data

        /* wifiSsid       */ WIFI_SSID,
        /* wifiPassword   */ WIFI_PASS,
        /* ntpServer      */ "pool.ntp.org",
        /* httpPort       */ 80,
    };
}
