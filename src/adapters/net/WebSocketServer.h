#pragma once

#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <string>
#include <type_traits>

#include "core/Messages.h"

namespace CE::Adapters
{
    /// WebSocket endpoint /ws/v1 (protocol v1, see docs/protocol.md).
    ///
    /// Network callbacks run in the async-TCP task, but the pump logic runs in the control task.
    /// So this class only decodes: valid commands go into a queue that the control task drains with
    /// Poll(); malformed messages are answered immediately without touching any pump state.
    class WebSocketServer
    {
    public:
        struct Incoming
        {
            uint32_t      clientId = 0;
            Core::Command command;
        };

        explicit WebSocketServer(uint16_t port = 80);
        ~WebSocketServer();

        WebSocketServer(const WebSocketServer&) = delete;
        WebSocketServer& operator=(const WebSocketServer&) = delete;

        /// Creates the queue and starts listening. Returns false if the queue cannot be created.
        [[nodiscard]] bool Begin();

        /// Non-blocking: takes the next queued command, if any.
        bool Poll(Incoming& out);

        void Send(uint32_t clientId, const std::string& message);
        void Broadcast(const std::string& message);

        /// Drops disconnected clients. Call from time to time.
        void Cleanup();

    private:
        static constexpr size_t kQueueLength = 4;
        static_assert(std::is_trivially_copyable_v<Incoming>, "Incoming is copied through a FreeRTOS queue");

        void OnEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t length);
        void OnText(AsyncWebSocketClient* client, const char* text, size_t length);

        AsyncWebServer _server;
        AsyncWebSocket _socket;
        QueueHandle_t _queue = nullptr;
    };
}
