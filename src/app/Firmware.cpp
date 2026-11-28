#include "app/Firmware.h"

#include <Arduino.h>

namespace CE::App
{
    namespace
    {
        // Stack sizes in bytes, priorities: the control task must win over the others.
        constexpr uint32_t kControlStack = 8192;
        constexpr uint32_t kSensorStack  = 4096;
        constexpr uint32_t kNetworkStack = 6144;
        constexpr UBaseType_t kControlPriority = 3;
        constexpr UBaseType_t kSensorPriority  = 2;
        constexpr UBaseType_t kNetworkPriority = 1;

        constexpr uint32_t kControlPeriodMs = 250;
        constexpr uint32_t kCleanupEveryLoops = 40;  // every 10 s
        constexpr size_t kSampleQueueLength = 8;

        // Serial 0 is the console, so anything but 1 selects UART2.
        HardwareSerial& SerialFor(const uint8_t number)
        {
            return number == 1 ? Serial1 : Serial2;
        }
    }

    Firmware::Firmware(const Profile& profile)
        : _relay(profile.pumpRelayPin),
          _xkcLow(profile.xkcLowPin),
          _xkcHigh(profile.xkcHighPin),
          _distanceType(profile.distanceSensor),
          _distancePwm(profile.distanceRxPin, profile.distanceTxPin),
          _distanceUart(profile.distanceRxPin, profile.distanceTxPin, SerialFor(profile.distanceSerial)),
          _distanceSensor(profile.distanceSensor == DistanceSensorType::A02YYUW_UART
                              ? static_cast<Core::IDistanceSensor&>(_distanceUart)
                              : static_cast<Core::IDistanceSensor&>(_distancePwm)),
          _clock(profile.ntpServer),
          _wifi(profile.wifiSsid, profile.wifiPassword),
          _server(profile.webSocketPort),
          _controller(_relay, _xkcLow, _xkcHigh, _clock, _store, _log, _server),
          _watchdog(_wifi, _restarter, _log)
    {
    }

    bool Firmware::Begin()
    {
        Serial.begin(115200);

        // Pins first: the relay must be off before anything else can go wrong.
        _relay.Begin();
        _xkcLow.Begin();
        _xkcHigh.Begin();

        // The distance sensor is optional: the XKC_Y23 sensors are the primary control, so a sensor
        // that cannot start is reported and the level just stays unknown.
        const bool distanceReady =
            _distanceType == DistanceSensorType::A02YYUW_UART ? _distanceUart.Begin() : _distancePwm.Begin();
        if (!distanceReady)
            _log.Log(Core::LogLevel::Error, kTag, "Distance sensor failed to start; level will be unknown");

        if (!_store.Begin())
            _log.Log(Core::LogLevel::Error, kTag, "Storage unavailable: settings and pump state will not persist");

        _controller.Begin();
        ShareSettings();

        // Network and time do not block: the pump stays in a "no time" fault until the clock syncs.
        _clock.Begin();
        _wifi.Begin();

        if (!_server.Begin())
        {
            _log.Log(Core::LogLevel::Error, kTag, "WebSocket server failed to start");
            return false;
        }

        _samples = xQueueCreate(kSampleQueueLength, sizeof(DistanceSample));
        if (_samples == nullptr)
        {
            _log.Log(Core::LogLevel::Error, kTag, "Cannot create sample queue");
            return false;
        }

        const bool started =
            xTaskCreate(ControlTask, "control", kControlStack, this, kControlPriority, nullptr) == pdPASS &&
            xTaskCreate(SensorTask, "sensor", kSensorStack, this, kSensorPriority, nullptr) == pdPASS &&
            xTaskCreate(NetworkTask, "network", kNetworkStack, this, kNetworkPriority, nullptr) == pdPASS;

        if (!started) _log.Log(Core::LogLevel::Error, kTag, "Cannot start tasks");
        return started;
    }

    void Firmware::ControlTask(void* self)
    {
        static_cast<Firmware*>(self)->ControlLoop();
    }

    void Firmware::SensorTask(void* self)
    {
        static_cast<Firmware*>(self)->SensorLoop();
    }

    void Firmware::NetworkTask(void* self)
    {
        static_cast<Firmware*>(self)->NetworkLoop();
    }

    void Firmware::ShareSettings()
    {
        const Core::Settings& settings = _controller.GetSettings();

        _clock.SetUtcOffsetMinutes(settings.utcOffsetMinutes);
        _sensorDelayMs = static_cast<uint32_t>(settings.radarDelayS) * 1000;
        _networkDelayMs = static_cast<uint32_t>(settings.watchdogDelayS) * 1000;
        _maxReconnects = static_cast<uint16_t>(settings.maxReconnects);
    }

    void Firmware::ControlLoop()
    {
        uint32_t loops = 0;

        for (;;)
        {
            DistanceSample sample;
            while (xQueueReceive(_samples, &sample, 0) == pdTRUE) _controller.OnDistance(sample);

            Adapters::WebSocketServer::Incoming incoming;
            while (_server.Poll(incoming)) _controller.OnCommand(incoming.clientId, incoming.command);

            ShareSettings();
            _controller.Update();

            if (++loops % kCleanupEveryLoops == 0)
            {
                _server.Cleanup();
                ESP_LOGD(kTag, "control task stack free (min): %u bytes", uxTaskGetStackHighWaterMark(nullptr));
            }

            vTaskDelay(pdMS_TO_TICKS(kControlPeriodMs));
        }
    }

    void Firmware::SensorLoop()
    {
        for (;;)
        {
            DistanceSample sample;
            sample.valid = _distanceSensor.Read(sample.cm);
            if (!sample.valid) sample.cm = 0.0f;

            // A full queue means the control task is behind: dropping the sample is fine.
            (void)xQueueSend(_samples, &sample, 0);

            vTaskDelay(pdMS_TO_TICKS(_sensorDelayMs.load()));
        }
    }

    void Firmware::NetworkLoop()
    {
        for (;;)
        {
            // Wait first: WiFi needs time to connect after boot.
            vTaskDelay(pdMS_TO_TICKS(_networkDelayMs.load()));

            Core::NetworkWatchdogConfig config;
            config.reconnectsBeforeRestart = _maxReconnects.load();
            _watchdog.SetConfig(config);
            _watchdog.Tick();
        }
    }
}
