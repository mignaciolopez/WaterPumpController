#include "adapters/net/RestApi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "adapters/codec/RestCodec.h"

namespace CE::Adapters
{
    namespace
    {
        constexpr const char* kJson = "application/json";
    }

    RestApi::RestApi(AsyncWebServer& server, CommandQueue& commands, const SnapshotCache& snapshot, Core::ILog& log)
        : _server(server), _commands(commands), _snapshot(snapshot), _log(log)
    {
    }

    void RestApi::Begin()
    {
        // Without this the library answers 500 for every path nobody handles.
        _server.onNotFound([this](AsyncWebServerRequest* request)
        {
            LogReceived(request, nullptr);
            Reply(request, Core::ErrorCode::UnknownCommand);  // HTTP 404
        });

        _server.on("/api/v1/state", HTTP_GET, [this](AsyncWebServerRequest* request)
        {
            LogReceived(request, nullptr);
            Answer(request, 200, _snapshot.State().c_str());
        });

        _server.on("/api/v1/settings", HTTP_GET, [this](AsyncWebServerRequest* request)
        {
            LogReceived(request, nullptr);
            Answer(request, 200, _snapshot.Settings().c_str());
        });

        auto onBody = [this](AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t index, size_t total)
        {
            OnBody(request, data, length, index, total);
        };

        _server.on("/api/v1/pump", HTTP_POST,
                   [this](AsyncWebServerRequest* request) { HandlePump(request); }, nullptr, onBody);

        _server.on("/api/v1/settings", HTTP_PUT,
                   [this](AsyncWebServerRequest* request) { HandleSettings(request); }, nullptr, onBody);
    }

    void RestApi::OnBody(AsyncWebServerRequest* request, const uint8_t* data, const size_t length, const size_t index,
                         const size_t total)
    {
        if (total == 0 || total > RestCodec::kMaxBodyBytes) return;  // the handler answers 400

        // The library frees _tempObject with free() when the request ends.
        if (index == 0) request->_tempObject = std::malloc(total + 1);
        if (request->_tempObject == nullptr || index + length > total) return;

        char* buffer = static_cast<char*>(request->_tempObject);
        std::memcpy(buffer + index, data, length);
        if (index + length == total) buffer[total] = '\0';
    }

    void RestApi::HandlePump(AsyncWebServerRequest* request)
    {
        HandleWrite(request, RestCodec::DecodePumpBody);
    }

    void RestApi::HandleSettings(AsyncWebServerRequest* request)
    {
        HandleWrite(request, RestCodec::DecodeSettingsBody);
    }

    template <typename Decoder>
    void RestApi::HandleWrite(AsyncWebServerRequest* request, Decoder decode)
    {
        const size_t length = request->contentLength();
        const char* body = static_cast<const char*>(request->_tempObject);
        LogReceived(request, body);

        if (!request->contentType().startsWith(kJson))
        {
            Answer(request, 415, RestCodec::EncodeError(Core::ErrorCode::BadRequest).c_str());
            return;
        }

        if (body == nullptr || length == 0 || length > RestCodec::kMaxBodyBytes)
        {
            Reply(request, Core::ErrorCode::BadRequest);
            return;
        }

        Core::Command command;
        const Core::ErrorCode error = decode(body, length, command);
        if (error != Core::ErrorCode::None)
        {
            Reply(request, error);
            return;
        }

        Submit(request, command);
    }

    void RestApi::Submit(AsyncWebServerRequest* request, const Core::Command& command)
    {
        RestTicket* ticket = RestTicket::Create();
        if (ticket == nullptr)
        {
            Reply(request, Core::ErrorCode::Internal);
            return;
        }

        Incoming incoming;
        incoming.command = command;
        incoming.ticket = ticket;

        if (!_commands.Push(incoming))
        {
            ticket->Release();  // the control task will never see it: drop both references
            ticket->Release();
            Reply(request, Core::ErrorCode::Internal);
            return;
        }

        const Core::ErrorCode result = ticket->Wait(kWriteTimeoutMs);
        ticket->Release();
        Reply(request, result);
    }

    void RestApi::Reply(AsyncWebServerRequest* request, const Core::ErrorCode result)
    {
        const std::string body = result == Core::ErrorCode::None ? RestCodec::EncodeOk() : RestCodec::EncodeError(result);
        Answer(request, RestCodec::HttpStatus(result), body.c_str());
    }

    void RestApi::LogReceived(AsyncWebServerRequest* request, const char* body)
    {
        char message[224];
        const String client = request->client() != nullptr ? request->client()->remoteIP().toString() : String("?");

        int length = std::snprintf(message, sizeof(message), "%s %s from %s", request->methodToString(),
                                   request->url().c_str(), client.c_str());
        if (body != nullptr && length > 0 && static_cast<size_t>(length) < sizeof(message))
            std::snprintf(message + length, sizeof(message) - length, " body: %.100s", body);

        _log.Log(Core::LogLevel::Info, "REST", message);
    }

    void RestApi::Answer(AsyncWebServerRequest* request, const int status, const char* body)
    {
        // A successful read would print the whole state/settings; show the answer only for writes and errors.
        const bool showBody = request->method() != HTTP_GET || status != 200;

        char message[224];
        std::snprintf(message, sizeof(message), "%s %s -> %d%s%.100s", request->methodToString(),
                      request->url().c_str(), status, showBody ? " " : "", showBody ? body : "");
        _log.Log(Core::LogLevel::Info, "REST", message);

        request->send(status, kJson, body);
    }
}
