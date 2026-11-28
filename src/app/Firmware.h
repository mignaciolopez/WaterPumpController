#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>
#include <cstdint>

#include "adapters/actuators/RelaySwitch.h"
#include "adapters/net/CommandQueue.h"
#include "adapters/net/HttpServer.h"
#include "adapters/net/RestApi.h"
#include "adapters/net/SnapshotCache.h"
#include "adapters/net/WebSocketServer.h"
#include "adapters/platform/EspLog.h"
#include "adapters/platform/EspRestarter.h"
#include "adapters/platform/SntpClock.h"
#include "adapters/platform/WifiNetwork.h"
#include "adapters/sensors/A02YYUW_PWM.h"
#include "adapters/sensors/A02YYUW_UART.h"
#include "adapters/sensors/XKC_Y23.h"
#include "adapters/storage/SpiffsStore.h"
#include "app/Controller.h"
#include "app/Profile.h"
#include "core/NetworkWatchdog.h"

namespace CE::App
{
    /// Composition root: builds the adapters, hands them to the Controller and runs three tasks.
    ///
    ///  - control task:  owns every core object (pump, tank, settings). Drains the sensor and command
    ///                   queues, runs the logic, publishes events.
    ///  - sensor task:   measures the distance (blocking) and passes samples through a queue.
    ///  - network task:  pings the gateway (blocking) and reconnects / restarts when needed.
    /// The tasks share nothing except the queues and a few atomics.
    class Firmware
    {
    public:
        explicit Firmware(const Profile& profile);

        Firmware(const Firmware&) = delete;
        Firmware& operator=(const Firmware&) = delete;

        /// Initializes hardware and starts the tasks. Returns false if something essential failed.
        [[nodiscard]] bool Begin();

    private:
        static void ControlTask(void* self);
        static void SensorTask(void* self);
        static void NetworkTask(void* self);

        void ControlLoop();
        void SensorLoop();
        void NetworkLoop();

        void ShareSettings();
        void UpdateSnapshot();
        void HandleIncoming(const Adapters::Incoming& incoming);

        static constexpr const char* kTag = "Firmware";

        Adapters::EspLog _log;
        Adapters::SpiffsStore _store;
        Adapters::RelaySwitch _relay;
        Adapters::XKC_Y23 _xkcLow;
        Adapters::XKC_Y23 _xkcHigh;

        // Both sensor drivers exist; the profile picks which one is started and used.
        DistanceSensorType _distanceType;
        Adapters::A02YYUW_PWM _distancePwm;
        Adapters::A02YYUW_UART _distanceUart;
        Core::IDistanceSensor& _distanceSensor;
        Adapters::SntpClock _clock;
        Adapters::WifiNetwork _wifi;
        Adapters::EspRestarter _restarter;
        Adapters::HttpServer _http;           // one server (ESP-IDF esp_http_server): WebSocket and REST share it
        Adapters::CommandQueue _commands;     // network tasks -> control task
        Adapters::SnapshotCache _snapshot;    // control task -> REST reads
        Adapters::WebSocketServer _server;
        Adapters::RestApi _rest;

        Controller _controller;
        Core::NetworkWatchdog _watchdog;

        QueueHandle_t _samples = nullptr;
        uint32_t _snapshotRevision = UINT32_MAX;  // revision the snapshot was built from

        // Copies of settings other tasks need; written by the control task only.
        std::atomic<uint32_t> _sensorDelayMs{1000};
        std::atomic<uint32_t> _networkDelayMs{60000};
        std::atomic<uint16_t> _maxReconnects{10};
    };
}
