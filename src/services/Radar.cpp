//
// Created by lmartinignacio@gmail.com on 4/11/2026.
//

#include <App.h>
#include <config/Build.hpp>
#include <config/Pins.hpp>
#include <os/Settings.h>
#include <os/Tasks.hpp>
#include <os/Time.hpp>
#include <services/Radar.h>
#include "../../include/services/WSServer.h"

namespace CE::Services
{
    Radar::Radar() :  _socketId(-1), _driver(Config::Pins::kEchoPin, Config::Pins::kTrigPin)
    {
        ESP_LOGV(TAG, "Constructor");
    }

    bool Radar::Setup()
    {
        ESP_LOGV(TAG, "Setup");

        if (!_driver.Setup())
        {
            return false;
        }

        bool result = OS::Tasks::Start(Task, TAG, Config::Build::kStackRadarTask, this, Config::Build::kPrioRadar, nullptr);
        //const bool watchdogResult = Watchdog::RegisterTask(TAG, Settings::Get().radarDelayS * 1000);

        if (result)
        {
            _socketId = WSServer::Instance().Subscribe([this](const JsonDocument& doc)
            {
                return this->HandleWebSocketMessage(doc);
            });
            //result = Watchdog::RegisterTask(TAG, Settings::Get().radarDelayS * 1000);
        }

        return result;
    }

    const Domain::RadarSample & Radar::GetSample() const
    {
        return _driver.sample;
    }

    void Radar::Task(void* pvParameters)
    {
        auto& radar = *static_cast<Radar*>(pvParameters);
        ESP_LOGV(radar.TAG, "Task Started");

        while (App::Instance().isRunning)
        {
            if (radar._driver.TryRead())
            {
                WSServer::Instance().Broadcast("distance", radar.GetSample());
                ESP_LOGD(radar.TAG, "Distance: '%.2f' cm.", radar._driver.sample.distanceCm);
            }
            else
            {
                ESP_LOGE(radar.TAG, "TryRead failed!");
            }

            OS::Time::SleepMs(OS::Settings::Get().radarDelayS * 1000);
        }

        ESP_LOGV(radar.TAG, "Task Finished");
    }

    bool Radar::HandleWebSocketMessage(const JsonDocument &doc)
    {
        const char* command = doc["command"];
        ESP_LOGD(TAG, "command: %s", command);

        if (strcmp(command, "getStatus") == 0)
        {
            WSServer::Instance().Broadcast("distance", GetSample());
            return false;
        }

        return false;
    }

}   // namespace CE::Services