//
// Created by lmartinignacio@gmail.com on 4/10/2026.
//

#include <App.h>
#include "os/Settings.h"
#include <services/NTP.hpp>
#include <services/WSServer.h>
#include <services/WiFi.hpp>

namespace CE
{
    App App::instance{};

    App& App::Instance()
    {
        return instance;
    }

    App::App() : isRunning(false), _pump({}), _radar({})
    {
        ESP_LOGI(TAG, "Constructor");
    }

    bool App::Setup()
    {
        ESP_LOGI(TAG, "Setup started.");

        isRunning = true;

        Serial.begin(115200);

        if (!OS::Settings::Setup())
        {
            ESP_LOGE(TAG, "Settings Setup failed");
            return false;
        }

        if (!Services::Connection::Setup())
        {
            return false;
        }

        if (!Services::NTP::Setup())
        {
            return false;
        }

        if (!Services::WSServer::Instance().Setup())
        {
            return false;
        }

        /*if (!_pump.Setup())
        {
            return false;
        }*/

        if (!_radar.Setup())
        {
            return false;
        }

        return true;
    }

    void App::Loop()
    {
        Services::WSServer::Instance().Loop();

        delay(1000);
    }
} // CE