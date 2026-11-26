//
// Created by lmartinignacio@gmail.com on 4/11/2026.
//

#include <App.h>
#include <algorithm>
#include <config/Build.hpp>
#include <config/Pins.hpp>
#include <os/Settings.h>
#include <os/Tasks.hpp>
#include <os/Time.hpp>
#include <services/Radar.h>
#include "../../include/services/WSServer.h"

namespace CE::Services
{
    Radar::Radar() :  _socketId(-1), _driver(Config::Pins::kEchoPin, Config::Pins::kTrigPin),
        _measurements{}, _measurementCount(0), _hasMedianSample(false), _sample{}
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
        return _sample;
    }

    void Radar::Task(void* pvParameters)
    {
        const auto& settings = OS::Settings::Get();
        auto& radar = *static_cast<Radar*>(pvParameters);
        ESP_LOGV(radar.TAG, "Task Started");

        while (App::Instance().isRunning)
        {
            if (radar._driver.TryRead())
            {
                radar._measurements[radar._measurementCount++] = radar._driver.sample.distanceCm;
                if (radar._measurementCount == settings.medianMeasurementCount)
                {
                    auto sortedMeasurements = radar._measurements;
                    std::sort(sortedMeasurements.begin(), sortedMeasurements.end());
                    radar._sample.distanceCm = sortedMeasurements[settings.medianMeasurementCount / 2];
                    radar._hasMedianSample = true;
                    radar._measurementCount = 0;

                    WSServer::Instance().Broadcast("distance", radar.GetSample());
                    ESP_LOGD(radar.TAG, "Median distance: '%.2f' cm.", radar._sample.distanceCm);
                }
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
            if (_hasMedianSample)
            {
                WSServer::Instance().Broadcast("distance", GetSample());
                return true;
            }
        }

        return false;
    }

}   // namespace CE::Services