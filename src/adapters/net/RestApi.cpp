#include "adapters/net/RestApi.h"

#include <cstdlib>
#include <cstring>

#include "adapters/codec/RestCodec.h"

namespace CE::Adapters
{
    namespace
    {
        constexpr const char* kJson = "application/json";
    }

    RestApi::RestApi(AsyncWebServer& server, CommandQueue& commands, const SnapshotCache& snapshot)
        : _server(server), _commands(commands), _snapshot(snapshot)
    {
    }

    void RestApi::Begin()
    {
        // Without this the library answers 500 for every path nobody handles.
        _server.onNotFound([](AsyncWebServerRequest* request)
        {
            Reply(request, Core::ErrorCode::UnknownCommand);  // HTTP 404
        });

        _server.on("/api/v1/state", HTTP_GET, [this](AsyncWebServerRequest* request)
        {
            request->send(200, kJson, _snapshot.State().c_str());
        });

        _server.on("/api/v1/settings", HTTP_GET, [this](AsyncWebServerRequest* request)
        {
            request->send(200, kJson, _snapshot.Settings().c_str());
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
        if (!request->contentType().startsWith(kJson))
        {
            request->send(415, kJson, RestCodec::EncodeError(Core::ErrorCode::BadRequest).c_str());
            return;
        }

        const size_t length = request->contentLength();
        const char* body = static_cast<const char*>(request->_tempObject);
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
        request->send(RestCodec::HttpStatus(result), kJson, body.c_str());
    }
}
