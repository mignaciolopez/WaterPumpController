//
// Created by lmart on 4/11/2026.
//

#pragma once

#include <ArduinoJson.h>

namespace CE::Services
{
    class Service
    {
    public:
        virtual ~Service() = default;

    private:
        virtual bool HandleWebSocketMessage(const JsonDocument& doc) = 0;
    };
}
