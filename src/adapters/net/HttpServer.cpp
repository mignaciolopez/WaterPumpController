#include "adapters/net/HttpServer.h"

#include <lwip/sockets.h>

#include <cstdio>
#include <cstring>

#include "adapters/codec/RestCodec.h"

namespace CE::Adapters
{
    namespace
    {
        // Static strings: httpd_resp_set_status keeps the pointer until the answer is sent.
        const char* StatusLine(const int status)
        {
            switch (status)
            {
                case 200: return "200 OK";
                case 400: return "400 Bad Request";
                case 404: return "404 Not Found";
                case 405: return "405 Method Not Allowed";
                case 409: return "409 Conflict";
                case 415: return "415 Unsupported Media Type";
                case 422: return "422 Unprocessable Entity";
                case 503: return "503 Service Unavailable";
                default:  return "500 Internal Server Error";
            }
        }

        HttpServer* Self(httpd_handle_t handle)
        {
            return static_cast<HttpServer*>(httpd_get_global_user_ctx(handle));
        }

        void NoFree(void*) {}  // the server object is owned by Firmware, not by the HTTP server
    }

    HttpServer::HttpServer(const uint16_t port, Core::ILog& log) : _port(port), _log(log)
    {
    }

    HttpServer::~HttpServer()
    {
        if (_handle != nullptr) httpd_stop(_handle);
    }

    bool HttpServer::Begin()
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = _port;
        config.stack_size = 10240;           // handlers use JSON and formatted logging
        config.max_open_sockets = 6;         // leaves room for lwIP's 10 sockets (ping, ...)
        config.max_uri_handlers = 8;
        config.lru_purge_enable = true;      // a new client replaces the oldest idle one
        config.global_user_ctx = this;
        config.global_user_ctx_free_fn = NoFree;
        config.close_fn = &HttpServer::OnClose;

        if (httpd_start(&_handle, &config) != ESP_OK)
        {
            _handle = nullptr;
            return false;
        }

        httpd_register_err_handler(_handle, HTTPD_404_NOT_FOUND, &HttpServer::OnNotFound);
        httpd_register_err_handler(_handle, HTTPD_405_METHOD_NOT_ALLOWED, &HttpServer::OnMethodNotAllowed);
        return true;
    }

    void HttpServer::PeerAddress(const int socketFd, char* out, const size_t size)
    {
        sockaddr_storage address = {};
        socklen_t length = sizeof(address);
        std::snprintf(out, size, "?");

        if (getpeername(socketFd, reinterpret_cast<sockaddr*>(&address), &length) != 0) return;

        if (address.ss_family == AF_INET)
        {
            inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(&address)->sin_addr, out, size);
        }
        else if (address.ss_family == AF_INET6)
        {
            inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6*>(&address)->sin6_addr, out, size);
            if (std::strncmp(out, "::ffff:", 7) == 0) std::memmove(out, out + 7, std::strlen(out + 7) + 1);  // IPv4 client
        }
    }

    void HttpServer::LogRequest(httpd_req_t* req, const char* body)
    {
        char client[48];
        PeerAddress(httpd_req_to_sockfd(req), client, sizeof(client));

        char message[224];
        const int length = std::snprintf(message, sizeof(message), "%s %s from %s",
                                         http_method_str(static_cast<http_method>(req->method)), req->uri, client);
        if (body != nullptr && length > 0 && static_cast<size_t>(length) < sizeof(message))
            std::snprintf(message + length, sizeof(message) - length, " body: %.100s", body);

        _log.Log(Core::LogLevel::Info, "REST", message);
    }

    esp_err_t HttpServer::Respond(httpd_req_t* req, const int status, const std::string& body)
    {
        // A successful read would print the whole state/settings; show the answer only for writes and errors.
        const bool showBody = req->method != HTTP_GET || status != 200;

        char message[224];
        std::snprintf(message, sizeof(message), "%s %s -> %d%s%.100s", http_method_str(static_cast<http_method>(req->method)),
                      req->uri, status, showBody ? " " : "", showBody ? body.c_str() : "");
        _log.Log(Core::LogLevel::Info, "REST", message);

        httpd_resp_set_status(req, StatusLine(status));
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, body.c_str(), static_cast<ssize_t>(body.size()));
    }

    void HttpServer::OnClose(httpd_handle_t handle, const int socketFd)
    {
        HttpServer* self = Self(handle);
        if (self != nullptr && self->_onClose) self->_onClose(socketFd);
    }

    esp_err_t HttpServer::OnNotFound(httpd_req_t* req, httpd_err_code_t)
    {
        HttpServer* self = Self(req->handle);
        self->LogRequest(req, nullptr);
        return self->Respond(req, 404, RestCodec::EncodeError(Core::ErrorCode::UnknownCommand));
    }

    esp_err_t HttpServer::OnMethodNotAllowed(httpd_req_t* req, httpd_err_code_t)
    {
        HttpServer* self = Self(req->handle);
        self->LogRequest(req, nullptr);
        return self->Respond(req, 405, RestCodec::EncodeError(Core::ErrorCode::BadRequest));
    }
}
