//
// Created by lmartinignacio@gmail.com on 4/11/2026.
//

#pragma once
//#include <driver/JSN-SR04M-2.h>
//#include <driver/HC-SR04.h>
//#include <driver/A02YYUW_UART.h>
#include <driver/A02YYUW_PWM.h>

namespace CE::Services
{
    class Radar : Service
    {
    public:
        Radar();
        [[nodiscard]] bool Setup();

        const Domain::RadarSample& GetSample() const;

    private:
        static void Task(void* pvParameters);
        bool HandleWebSocketMessage(const JsonDocument& doc) override;

    private:
        const char* TAG = "Service-Radar";
        size_t _socketId;
        Driver::A02YYUW_PWM _driver;
    };

}   // namespace CE::Services