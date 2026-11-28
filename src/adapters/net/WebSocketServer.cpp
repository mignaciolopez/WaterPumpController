#include "adapters/net/WebSocketServer.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <new>

#include "adapters/codec/CodecV1.h"

namespace CE::Adapters
{
    // A message waiting for the server task. clientId < 0 means every client.
    struct WebSocketServer::Outgoing
    {
        WebSocketServer* self;
        int clientId;
        std::string message;
    };

    WebSocketServer::WebSocketServer(HttpServer& http, CommandQueue& commands, Core::ILog& log)
        : _http(http), _commands(commands), _log(log)
    {
    }

    bool WebSocketServer::Begin()
    {
        _http.SetCloseHandler([this](const int socketFd) { OnClosed(socketFd); });

        httpd_uri_t route = {};
        route.uri = "/ws/v1";
        route.method = HTTP_GET;
        route.handler = &WebSocketServer::OnRequest;
        route.user_ctx = this;
        route.is_websocket = true;
        route.handle_ws_control_frames = false;  // the server answers ping and close by itself
        route.supported_subprotocol = nullptr;

        return httpd_register_uri_handler(_http.Handle(), &route) == ESP_OK;
    }

    // ---- sending: from any task; the real send happens in the server task

    void WebSocketServer::Send(const uint32_t clientId, const std::string& message)
    {
        LogMessage("answered", static_cast<int>(clientId), message.c_str(), message.size());
        Queue(static_cast<int>(clientId), message);
    }

    void WebSocketServer::Broadcast(const std::string& message)
    {
        Queue(-1, message);
    }

    void WebSocketServer::Queue(const int clientId, const std::string& message)
    {
        auto* outgoing = new (std::nothrow) Outgoing{this, clientId, message};
        if (outgoing == nullptr) return;

        if (httpd_queue_work(_http.Handle(), &WebSocketServer::Deliver, outgoing) != ESP_OK) delete outgoing;
    }

    void WebSocketServer::Deliver(void* arg)  // runs in the server task
    {
        const std::unique_ptr<Outgoing> outgoing(static_cast<Outgoing*>(arg));
        WebSocketServer* self = outgoing->self;

        if (outgoing->clientId >= 0)
        {
            self->SendFrame(outgoing->clientId, outgoing->message);
            return;
        }

        const std::vector<int> clients = self->_clients;  // a failed send may close a connection while we iterate
        for (const int clientId : clients) self->SendFrame(clientId, outgoing->message);
    }

    void WebSocketServer::SendFrame(const int clientId, const std::string& message)  // server task
    {
        if (std::find(_clients.begin(), _clients.end(), clientId) == _clients.end()) return;  // already gone

        httpd_ws_frame_t frame = {};
        frame.type = HTTPD_WS_TYPE_TEXT;
        frame.payload = reinterpret_cast<uint8_t*>(const_cast<char*>(message.data()));
        frame.len = message.size();

        if (httpd_ws_send_frame_async(_http.Handle(), clientId, &frame) != ESP_OK)
            _log.Log(Core::LogLevel::Debug, "WS", "Send failed (client probably gone)");
    }

    // ---- receiving: server task

    esp_err_t WebSocketServer::OnRequest(httpd_req_t* req)
    {
        auto* self = static_cast<WebSocketServer*>(req->user_ctx);
        return req->method == HTTP_GET ? self->HandleHandshake(req) : self->HandleFrame(req);
    }

    esp_err_t WebSocketServer::HandleHandshake(httpd_req_t* req)
    {
        const int clientId = httpd_req_to_sockfd(req);
        if (std::find(_clients.begin(), _clients.end(), clientId) == _clients.end()) _clients.push_back(clientId);

        char address[48];
        HttpServer::PeerAddress(clientId, address, sizeof(address));
        char message[96];
        std::snprintf(message, sizeof(message), "Client #%d connected from %s", clientId, address);
        _log.Log(Core::LogLevel::Info, "WS", message);
        return ESP_OK;
    }

    esp_err_t WebSocketServer::HandleFrame(httpd_req_t* req)
    {
        const int clientId = httpd_req_to_sockfd(req);

        httpd_ws_frame_t frame = {};
        frame.type = HTTPD_WS_TYPE_TEXT;
        esp_err_t error = httpd_ws_recv_frame(req, &frame, 0);  // first only the length
        if (error != ESP_OK) return error;

        if (frame.len > CodecV1::kMaxMessageBytes)
        {
            _log.Log(Core::LogLevel::Warn, "WS", "Message too large, closing the connection");
            return ESP_FAIL;  // the rest of the frame is unread: the connection cannot be used any more
        }

        std::string text(frame.len, '\0');
        frame.payload = reinterpret_cast<uint8_t*>(text.data());
        error = httpd_ws_recv_frame(req, &frame, frame.len);
        if (error != ESP_OK) return error;

        if (frame.type != HTTPD_WS_TYPE_TEXT || frame.fragmented || !frame.final)
        {
            // Fragmented or binary messages are not part of protocol v1.
            Send(static_cast<uint32_t>(clientId), CodecV1::EncodeError(Core::Command::kNoId, Core::ErrorCode::BadRequest));
            return ESP_OK;
        }

        OnText(clientId, text.data(), text.size());
        return ESP_OK;
    }

    void WebSocketServer::OnText(const int clientId, const char* text, const size_t length)
    {
        LogMessage("received", clientId, text, length);

        const CodecV1::DecodeResult decoded = CodecV1::DecodeCommand(text, length);
        if (decoded.error != Core::ErrorCode::None)
        {
            Send(static_cast<uint32_t>(clientId), CodecV1::EncodeError(decoded.command.id, decoded.error));
            return;
        }

        Incoming incoming;
        incoming.clientId = static_cast<uint32_t>(clientId);
        incoming.command = decoded.command;

        if (!_commands.Push(incoming))
            Send(static_cast<uint32_t>(clientId), CodecV1::EncodeError(decoded.command.id, Core::ErrorCode::Internal));
    }

    void WebSocketServer::OnClosed(const int socketFd)  // server task
    {
        const auto it = std::find(_clients.begin(), _clients.end(), socketFd);
        if (it == _clients.end()) return;  // a plain HTTP connection

        _clients.erase(it);
        char message[64];
        std::snprintf(message, sizeof(message), "Client #%d disconnected", socketFd);
        _log.Log(Core::LogLevel::Info, "WS", message);
    }

    void WebSocketServer::LogMessage(const char* what, const int clientId, const char* text, const size_t length)
    {
        constexpr size_t kShown = 100;  // long answers (the settings) are cut

        char message[200];
        std::snprintf(message, sizeof(message), "WS #%d %s: %.*s%s", clientId, what,
                      static_cast<int>(std::min(length, kShown)), text, length > kShown ? "..." : "");
        _log.Log(Core::LogLevel::Info, "WS", message);
    }
}
