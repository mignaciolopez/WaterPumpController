#include "adapters/net/WebSocketServer.h"

#include <algorithm>
#include <cstdio>

#include "adapters/codec/CodecV1.h"

namespace CE::Adapters
{
    WebSocketServer::WebSocketServer(AsyncWebServer& server, CommandQueue& commands, Core::ILog& log)
        : _server(server), _commands(commands), _log(log), _socket("/ws/v1")
    {
    }

    void WebSocketServer::Begin()
    {
        _socket.onEvent([this](AsyncWebSocket*, AsyncWebSocketClient* client, const AwsEventType type, void* arg,
                               uint8_t* data, const size_t length)
        {
            OnEvent(client, type, arg, data, length);
        });
        _server.addHandler(&_socket);
    }

    void WebSocketServer::LogMessage(const char* what, const uint32_t clientId, const char* text, const size_t length)
    {
        constexpr size_t kShown = 100;  // long answers (the settings) are cut

        char message[200];
        std::snprintf(message, sizeof(message), "WS #%u %s: %.*s%s", static_cast<unsigned>(clientId), what,
                      static_cast<int>(std::min(length, kShown)), text, length > kShown ? "..." : "");
        _log.Log(Core::LogLevel::Info, "WS", message);
    }

    void WebSocketServer::Send(const uint32_t clientId, const std::string& message)
    {
        LogMessage("answered", clientId, message.c_str(), message.size());
#ifdef WS_DISABLE_SENDS
        return;  // experiment build: nothing is ever sent on a WebSocket
#endif
        _socket.text(clientId, message.c_str(), message.size());
    }

    void WebSocketServer::Broadcast(const std::string& message)
    {
#ifdef WS_DISABLE_SENDS
        (void)message;
        return;  // experiment build: nothing is ever sent on a WebSocket
#endif
        _socket.textAll(message.c_str(), message.size());
    }

    void WebSocketServer::Cleanup()
    {
        _socket.cleanupClients();
    }

    void WebSocketServer::OnEvent(AsyncWebSocketClient* client, const AwsEventType type, void* arg, uint8_t* data,
                                  const size_t length)
    {
        if (type == WS_EVT_CONNECT || type == WS_EVT_DISCONNECT)
        {
            char message[96];
            if (type == WS_EVT_CONNECT)
                std::snprintf(message, sizeof(message), "Client #%u connected from %s", static_cast<unsigned>(client->id()),
                              client->remoteIP().toString().c_str());
            else
                std::snprintf(message, sizeof(message), "Client #%u disconnected", static_cast<unsigned>(client->id()));
            _log.Log(Core::LogLevel::Info, "WS", message);
            return;
        }

        if (type != WS_EVT_DATA) return;

        const auto* info = static_cast<const AwsFrameInfo*>(arg);
        const bool singleTextFrame = info->final && info->index == 0 && info->len == length && info->opcode == WS_TEXT;
        if (!singleTextFrame)
        {
            // Fragmented or binary messages are not part of protocol v1.
            Send(client->id(), CodecV1::EncodeError(Core::Command::kNoId, Core::ErrorCode::BadRequest));
            return;
        }

        OnText(client, reinterpret_cast<const char*>(data), length);
    }

    void WebSocketServer::OnText(AsyncWebSocketClient* client, const char* text, const size_t length)
    {
        LogMessage("received", client->id(), text, length);

        const CodecV1::DecodeResult decoded = CodecV1::DecodeCommand(text, length);
        if (decoded.error != Core::ErrorCode::None)
        {
            Send(client->id(), CodecV1::EncodeError(decoded.command.id, decoded.error));
            return;
        }

        Incoming incoming;
        incoming.clientId = client->id();
        incoming.command = decoded.command;

        if (!_commands.Push(incoming))
            Send(client->id(), CodecV1::EncodeError(decoded.command.id, Core::ErrorCode::Internal));
    }
}
