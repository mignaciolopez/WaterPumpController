#include "app/Firmware.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "adapters/codec/RestCodec.h"

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

        // Debug aid: walk the whole heap this often and log as soon as it is damaged, so the damage can be
        // matched with the REST/WS lines just before it. 0 turns it off.
        constexpr uint32_t kHeapCheckEveryLoops = 8;  // every 2 s
        constexpr size_t kSampleQueueLength = 8;
        constexpr size_t kWeatherQueueLength = 2;
        constexpr uint32_t kFirstWeatherReadDelayMs = 2000;  // a DHT needs a moment after power-up

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
          _http(profile.httpPort, _log),
          _server(_http, _commands, _log),
          _rest(_http, _commands, _snapshot),
          _controller(_relay, _xkcLow, _xkcHigh, _clock, _store, _log, _server),
          _watchdog(_wifi, _restarter, _log)
    {
        if (profile.weatherSensor != WeatherSensorType::None)
        {
            _dht.emplace(profile.weatherPin, profile.weatherSensor == WeatherSensorType::Dht22 ? DHT22 : DHT11);
            _weatherSensor = &*_dht;
        }
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

        if (_dht) _dht->Begin();

        if (!_store.Begin())
            _log.Log(Core::LogLevel::Error, kTag, "Storage unavailable: settings and pump state will not persist");

        _controller.Begin();
        _controller.SetWeatherSensorPresent(_weatherSensor != nullptr);
        _controller.SetSensorRange({_distanceSensor.MinDistanceCm(), _distanceSensor.MaxDistanceCm()});
        ShareSettings();

        // Network and time do not block: the pump stays in a "no time" fault until the clock syncs.
        _clock.Begin();
        _wifi.Begin();

        if (!_commands.Begin())
        {
            _log.Log(Core::LogLevel::Error, kTag, "Cannot create command queue");
            return false;
        }

        UpdateSnapshot();  // so a REST read before the first control loop already has data
        if (!_http.Begin() || !_server.Begin() || !_rest.Begin())  // start, then register /ws/v1 and /api/v1/...
        {
            _log.Log(Core::LogLevel::Error, kTag, "HTTP server failed to start");
            return false;
        }
        _log.Log(Core::LogLevel::Info, kTag, "HTTP server started: WebSocket /ws/v1, REST /api/v1");

        _samples = xQueueCreate(kSampleQueueLength, sizeof(DistanceSample));
        if (_weatherSensor != nullptr) _weatherSamples = xQueueCreate(kWeatherQueueLength, sizeof(Core::WeatherSample));
        if (_samples == nullptr || (_weatherSensor != nullptr && _weatherSamples == nullptr))
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
        _weatherDelayMs = static_cast<uint32_t>(settings.weatherDelayS) * 1000;
        _networkDelayMs = static_cast<uint32_t>(settings.watchdogDelayS) * 1000;
        _maxReconnects = static_cast<uint16_t>(settings.maxReconnects);
    }

    void Firmware::UpdateSnapshot()
    {
        if (_controller.Revision() == _snapshotRevision) return;

        _snapshotRevision = _controller.Revision();
        _snapshot.Set(
            Adapters::RestCodec::EncodeState(_controller.GetPumpState(), _controller.HasLevel(), _controller.LevelCm(),
                                             _controller.GetSensorRange(), _controller.WeatherOrNull()),
            Adapters::RestCodec::EncodeSettings(_controller.GetSettings()));
    }

    void Firmware::HandleIncoming(const Adapters::Incoming& incoming)
    {
        if (incoming.ticket == nullptr)
        {
            _controller.OnCommand(incoming.clientId, incoming.command);  // WebSocket: answered by the controller
            return;
        }

        // REST: run it, refresh the cache, and only then wake the HTTP handler, so a GET right after
        // the reply already sees the result.
        const Core::CommandOutcome outcome = _controller.Execute(incoming.command);
        UpdateSnapshot();
        incoming.ticket->Complete(outcome.error);
        incoming.ticket->Release();
    }

    void Firmware::ControlLoop()
    {
        uint32_t loops = 0;
        bool heapDamaged = false;

        for (;;)
        {
            DistanceSample sample;
            while (xQueueReceive(_samples, &sample, 0) == pdTRUE) _controller.OnDistance(sample);

            Core::WeatherSample weather;
            while (_weatherSamples != nullptr && xQueueReceive(_weatherSamples, &weather, 0) == pdTRUE)
                _controller.OnWeather(weather);

            // Waiting for a command is also this task's sleep: commands are handled right away,
            // and the logic still runs every kControlPeriodMs when nothing arrives.
            Adapters::Incoming incoming;
            bool received = _commands.Poll(incoming, kControlPeriodMs);
            while (received)
            {
                HandleIncoming(incoming);
                received = _commands.Poll(incoming, 0);
            }

            ShareSettings();
            _controller.Update();
            UpdateSnapshot();

            if (kHeapCheckEveryLoops != 0 && loops % kHeapCheckEveryLoops == 0 && !heapDamaged &&
                !heap_caps_check_integrity_all(true))
            {
                heapDamaged = true;  // the details were printed above; log the rest only once
                _log.Log(Core::LogLevel::Error, kTag,
                         "HEAP CORRUPTION DETECTED: something wrote outside its memory. See the REST/WS lines before this one.");
            }

            if (++loops % kCleanupEveryLoops == 0)
            {
                ESP_LOGD(kTag, "control task stack free (min): %u bytes; heap free %u, lowest %u", uxTaskGetStackHighWaterMark(nullptr),
                         static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMinFreeHeap()));
            }
        }
    }

    void Firmware::SensorLoop()
    {
        uint32_t nextWeatherAt = millis() + kFirstWeatherReadDelayMs;

        for (;;)
        {
            const DistanceSample sample = ToSample(_distanceSensor.Read(), _distanceSensor.MinDistanceCm());

            // A full queue means the control task is behind: dropping the sample is fine.
            (void)xQueueSend(_samples, &sample, 0);

            // Temperature and humidity change slowly: read them every weatherDelayS (signed difference: millis() wraps).
            if (_weatherSensor != nullptr && static_cast<int32_t>(millis() - nextWeatherAt) >= 0)
            {
                const Core::WeatherSample weather = _weatherSensor->Read();
                (void)xQueueSend(_weatherSamples, &weather, 0);
                nextWeatherAt = millis() + _weatherDelayMs.load();
            }

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
