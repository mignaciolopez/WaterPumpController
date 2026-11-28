#pragma once

#include <esp_http_server.h>

#include <cstdint>
#include <functional>
#include <string>

#include "core/ports/ILog.h"

namespace CE::Adapters
{
    /// The HTTP server (ESP-IDF esp_http_server) that serves both the REST API and the WebSocket.
    ///
    /// All socket work happens in the server's own task: request handlers, the close callback and the
    /// functions queued with httpd_queue_work. Other tasks must only hand work over (see WebSocketServer),
    /// never touch a socket directly.
    ///
    /// Unknown paths answer 404 and wrong methods 405, both as JSON ({"ok":false,"err":"..."}).
    class HttpServer
    {
    public:
        HttpServer(uint16_t port, Core::ILog& log);
        ~HttpServer();

        HttpServer(const HttpServer&) = delete;
        HttpServer& operator=(const HttpServer&) = delete;

        /// Starts listening. Register the routes afterwards.
        [[nodiscard]] bool Begin();

        httpd_handle_t Handle() const { return _handle; }

        /// Called in the server task whenever a connection closes (the file descriptor may be invalid).
        /// HttpServer closes the socket right after the handler returns (esp_http_server does not when a close
        /// callback is set), so the handler must not.
        void SetCloseHandler(std::function<void(int)> handler) { _onClose = std::move(handler); }

        /// Logs "<METHOD> <uri> from <ip>" (tag REST), plus the request body for writes.
        void LogRequest(httpd_req_t* req, const char* body);

        /// Sends a JSON answer and logs "<METHOD> <uri> -> <status>" (the body too for writes and errors).
        esp_err_t Respond(httpd_req_t* req, int status, const std::string& body);

        /// Text form of a socket's remote address ("192.168.100.3"), "?" when unknown.
        static void PeerAddress(int socketFd, char* out, size_t size);

    private:
        static void OnClose(httpd_handle_t handle, int socketFd);
        static esp_err_t OnNotFound(httpd_req_t* req, httpd_err_code_t error);
        static esp_err_t OnMethodNotAllowed(httpd_req_t* req, httpd_err_code_t error);

        uint16_t _port;
        Core::ILog& _log;
        httpd_handle_t _handle = nullptr;
        std::function<void(int)> _onClose;
    };
}
