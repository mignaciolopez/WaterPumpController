#pragma once

#include <esp_http_server.h>

#include <cstdint>
#include <string>
#include <vector>

#include "adapters/net/CommandQueue.h"
#include "adapters/net/HttpServer.h"
#include "core/ports/ILog.h"
#include "core/ports/IMessageSink.h"

namespace CE::Adapters
{
    /// WebSocket endpoint /ws/v1 (protocol v1, see docs/protocol.md) on the HTTP server.
    ///
    /// Thread rules: the connection list and every socket call belong to the HTTP server task. Incoming
    /// messages are decoded there and pushed into the command queue for the control task. The control task
    /// answers through Send()/Broadcast(), which only queue the message with httpd_queue_work; the server
    /// task then writes it. So no socket is ever touched from two tasks.
    ///
    /// A client's id is its socket number. Logged (tag "WS", Info): clients connecting and leaving, every
    /// command received and every answer sent. Pushed events (Broadcast) are not logged: the level event
    /// alone would fire about once a second.
    class WebSocketServer final : public Core::IMessageSink
    {
    public:
        WebSocketServer(HttpServer& http, CommandQueue& commands, Core::ILog& log);

        WebSocketServer(const WebSocketServer&) = delete;
        WebSocketServer& operator=(const WebSocketServer&) = delete;

        /// Registers the endpoint (the server must be started).
        [[nodiscard]] bool Begin();

        void Send(uint32_t clientId, const std::string& message) override;
        void Broadcast(const std::string& message) override;

    private:
        struct Outgoing;

        static esp_err_t OnRequest(httpd_req_t* req);
        static void Deliver(void* arg);

        esp_err_t HandleHandshake(httpd_req_t* req);
        esp_err_t HandleFrame(httpd_req_t* req);
        void OnText(int clientId, const char* text, size_t length);
        void OnClosed(int socketFd);
        void Queue(int clientId, const std::string& message);
        void SendFrame(int clientId, const std::string& message);
        void LogMessage(const char* what, int clientId, const char* text, size_t length);

        HttpServer& _http;
        CommandQueue& _commands;
        Core::ILog& _log;
        std::vector<int> _clients;  // only touched in the HTTP server task
    };
}
