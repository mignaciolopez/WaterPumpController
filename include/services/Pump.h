//
// Created by lmart on 4/10/2026.
//

#pragma once

#include <Services/Service.h>
#include "domain/States.hpp"
#include "driver/Relay.h"
#include "driver/XKC_Y23.h"
#include "os/Persistent.hpp"

namespace CE::Services
{
    class Pump : Service
    {
    public:
        Pump();

        [[nodiscard]] bool Setup();

        void Switch(bool on);
        const Domain::States::Pump& GetState() const;

    private:
        static void Task(void* pvParameters);
        bool HandleWebSocketMessage(const JsonDocument& doc);

        void SwitchOn();
        void SwitchOff();

        void UpdateTimeOn();
        void UpdateCooldown();

    private:
        const char* TAG = "Service-Pump";

        Driver::Relay _relay;
        Driver::XKC_Y23 _xkcLow;
        Driver::XKC_Y23 _xkcHigh;

        size_t _socketId;

        Domain::States::Pump _state;
        OS::Persistent<Domain::States::Pump> _persistent;
    };
}
