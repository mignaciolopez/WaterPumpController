//
// Created by lmart on 4/10/2026.
//

#include <App.h>
#include <config/Build.hpp>
#include <config/Pins.hpp>
#include "os/Settings.h"
#include <os/Tasks.hpp>
#include "os/Time.hpp"
#include <services/Pump.h>
#include <services/WSServer.h>

namespace CE::Services
{
    Pump::Pump() :  _relay(Config::Pins::kPumpSwitchPin),
                    _xkcLow(Config::Pins::kPumpXKCHigh, "Driver-XKC_Y23-High"),
                    _xkcHigh(Config::Pins::kPumpXKCLow, "Driver-XKC_Y23-Low"),
                    _socketId(-1),
                    _state({}),
                    _persistent(_state)
    {
        ESP_LOGV(TAG, "Constructor");
    }

    bool Pump::Setup()
    {
        try
        {
            if (!_relay.Setup())
                return false;

            if (!_xkcLow.Setup())
                return false;

            if (!_xkcHigh.Setup())
                return false;

            if (!_persistent.Read())
                _persistent.Save();

            bool result = OS::Tasks::Start(Task, TAG, Config::Build::kStackPumpTask, this, Config::Build::kPrioPump, nullptr);
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
        catch (std::exception& e)
        {
            ESP_LOGE(TAG, "exception: %s", e.what());
            return false;
        }
    }

    void Pump::Switch(const bool on)
    {
        if (on) SwitchOn();
        else SwitchOff();
    }

    const Domain::States::Pump& Pump::GetState() const
    {
        return _state;
    }

    void Pump::Task(void *pvParameters)
    {
        auto& pump = *static_cast<Pump*>(pvParameters);

        while (App::Instance().isRunning)
        {
            if (pump._xkcHigh.IsDetectingWater())
                pump.SwitchOff();
            else if (!pump._xkcLow.IsDetectingWater())
                pump.SwitchOn();

            pump.UpdateTimeOn();
            pump.UpdateCooldown();

            //Watchdog::NotifyTaskAlive(TAG);
            OS::Time::SleepMs(OS::Settings::Get().radarDelayS * 1000);
        }

        WSServer::Instance().Unsubscribe(pump._socketId);
    }

    bool Pump::HandleWebSocketMessage(const JsonDocument &doc)
    {
        const char* command = doc["command"];
        ESP_LOGD(TAG, "command: %s", command);

        if (strcmp(command, "getStatus") == 0)
        {
            WSServer::Instance().Broadcast("pumpStatus", _state);
            return false;
        }

        if (strcmp(command, "getPumpStatus") == 0)
        {
            WSServer::Instance().Broadcast("pumpStatus", _state);
            return true;
        }

        if (strcmp(command, "setPump") == 0)
        {
            Switch(doc["state"]);
            return true;
        }

        return false;
    }

    void Pump::SwitchOn()
    {
        if (_xkcHigh.IsDetectingWater())
        {
            ESP_LOGI(TAG, "High Sensor is detecting water, can not switch On.");
            return;
        }

        if (!_state.isOn && !_state.isOnCooldown)
        {
            if (OS::Time::Get() - _state.timeStampOff < 10)
            {
                ESP_LOGW(TAG, "Can not switch pump to fast!");
                return;
            }

            _state.timeStampOn = OS::Time::Get();
            ESP_LOGI(TAG, "Pump switched ON at: %s", OS::Time::GetFormattedTime());
            _relay.SwitchOn();
            _state.isOn = true;
            _persistent.Save();
            WSServer::Instance().Broadcast("pumpStatus", _state);
        }
    }

    void Pump::SwitchOff()
    {
        if (_state.isOn)
        {
            if (OS::Time::Get() - _state.timeStampOn < 10)
            {
                ESP_LOGW(TAG, "Can not switch pump to fast!");
                return;
            }

            _state.timeStampOff = OS::Time::Get();
            ESP_LOGI(TAG, "Pump switched OFF at: %s", OS::Time::GetFormattedTime());
            _relay.SwitchOff();
            _state.isOn = false;
            _persistent.Save();
            WSServer::Instance().Broadcast("pumpStatus", _state);
        }
    }

    void Pump::UpdateTimeOn()
    {
        if (_state.isOn)
        {
            const auto& s = OS::Settings::Get();
            ESP_LOGI(TAG, "UpdateTimeOn: On at: %d should turn of in: %d seconds.", _state.timeStampOn, s.pumpMaxTimeOnM * 60 - (OS::Time::Get() - _state.timeStampOn));
            if (OS::Time::Get() - _state.timeStampOn > s.pumpMaxTimeOnM * 60)
            {
                SwitchOff();
                _state.isOnCooldown = true;
                _state.timeStampCooldown = OS::Time::Get();
                //Watchdog::ReportError(Domain::ErrorSeverity::Error, Domain::ErrorType::PumpTimeout, TAG, "Pump timeout");
                _persistent.Save();
            }
        }
    }

    void Pump::UpdateCooldown()
    {
        if (_state.isOnCooldown)
        {
            const auto& s = OS::Settings::Get();
            ESP_LOGI(TAG, "UpdateCooldown at: %d should release pump in: %d seconds.", _state.timeStampCooldown, s.pumpCooldownTimeM * 60 - (OS::Time::Get() - _state.timeStampCooldown));
            if (OS::Time::Get() - _state.timeStampCooldown > s.pumpCooldownTimeM * 60)
            {
                _state.isOnCooldown = false;
                _persistent.Save();
            }
        }
    }
}
