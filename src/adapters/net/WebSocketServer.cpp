#include "adapters/net/WebSocketServer.h"

#include "adapters/codec/CodecV1.h"

namespace CE::Adapters
{
    WebSocketServer::WebSocketServer(const uint16_t port) : _server(port), _socket("/ws/v1")
    {
    }

    WebSocketServer::~WebSocketServer()
    {
        if (_queue != nullptr) vQueueDelete(_queue);
    }

    bool WebSocketServer::Begin()
    {
        _queue = xQueueCreate(kQueueLength, sizeof(Incoming));
        if (_queue == nullptr) return false;

        _socket.onEvent([this](AsyncWebSocket*, AsyncWebSocketClient* client, const AwsEventType type, void* arg,
                               uint8_t* data, const size_t length)
        {
            OnEvent(client, type, arg, data, length);
        });
        _server.addHandler(&_socket);
        _server.begin();
        return true;
    }

    bool WebSocketServer::Poll(Incoming& out)
    {
        return _queue != nullptr && xQueueReceive(_queue, &out, 0) == pdTRUE;
    }

    void WebSocketServer::Send(const uint32_t clientId, const std::string& message)
    {
        _socket.text(clientId, message.c_str(), message.size());
    }

    void WebSocketServer::Broadcast(const std::string& message)
    {
        _socket.textAll(message.c_str(), message.size());
    }

    void WebSocketServer::Cleanup()
    {
        _socket.cleanupClients();
    }

    void WebSocketServer::OnEvent(AsyncWebSocketClient* client, const AwsEventType type, void* arg, uint8_t* data,
                                  const size_t length)
    {
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
        const CodecV1::DecodeResult decoded = CodecV1::DecodeCommand(text, length);
        if (decoded.error != Core::ErrorCode::None)
        {
            Send(client->id(), CodecV1::EncodeError(decoded.command.id, decoded.error));
            return;
        }

        Incoming incoming;
        incoming.clientId = client->id();
        incoming.command = decoded.command;

        if (xQueueSend(_queue, &incoming, 0) != pdTRUE)
            Send(client->id(), CodecV1::EncodeError(decoded.command.id, Core::ErrorCode::Internal));
    }
}
