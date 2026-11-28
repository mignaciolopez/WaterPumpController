#pragma once

#include <ESPAsyncWebServer.h>

#include <cstdint>
#include <string>

#include "adapters/net/CommandQueue.h"
#include "core/ports/IMessageSink.h"

namespace CE::Adapters
{
    /// WebSocket endpoint /ws/v1 (protocol v1, see docs/protocol.md).
    ///
    /// Network callbacks run in the async-TCP task, but the pump logic runs in the control task.
    /// So this class only decodes: valid commands go into the shared command queue; malformed
    /// messages are answered immediately without touching any pump state.
    class WebSocketServer final : public Core::IMessageSink
    {
    public:
        WebSocketServer(AsyncWebServer& server, CommandQueue& commands);

        WebSocketServer(const WebSocketServer&) = delete;
        WebSocketServer& operator=(const WebSocketServer&) = delete;

        /// Registers the endpoint. The caller starts the server.
        void Begin();

        void Send(uint32_t clientId, const std::string& message) override;
        void Broadcast(const std::string& message) override;

        /// Drops disconnected clients. Call from time to time.
        void Cleanup();

    private:
        void OnEvent(AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t length);
        void OnText(AsyncWebSocketClient* client, const char* text, size_t length);

        AsyncWebServer& _server;
        CommandQueue& _commands;
        AsyncWebSocket _socket;
    };
}
