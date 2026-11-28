#pragma once

#include <ESPAsyncWebServer.h>

#include "adapters/net/CommandQueue.h"
#include "adapters/net/SnapshotCache.h"
#include "core/Messages.h"
#include "core/ports/ILog.h"

namespace CE::Adapters
{
    /// REST interface under /api/v1 (see docs/rest-api.md).
    ///
    ///   GET  /api/v1/state      latest state, from the cache (never blocks on the pump logic)
    ///   GET  /api/v1/settings   latest settings, from the cache
    ///   POST /api/v1/pump       {"on":true}   goes through the command queue
    ///   PUT  /api/v1/settings   {"minLevelCm":90,...}   goes through the command queue
    ///
    /// A write waits (up to 2 s) for the control task to run it, then answers with its result.
    /// Writes must have Content-Type: application/json, so a web page on another site cannot trigger
    /// them with a "simple" cross-site request.
    class RestApi
    {
    public:
        RestApi(AsyncWebServer& server, CommandQueue& commands, const SnapshotCache& snapshot, Core::ILog& log);

        /// Registers the routes. The caller starts the server.
        void Begin();

    private:
        static constexpr uint32_t kWriteTimeoutMs = 2000;

        void OnBody(AsyncWebServerRequest* request, const uint8_t* data, size_t length, size_t index, size_t total);
        void HandlePump(AsyncWebServerRequest* request);
        void HandleSettings(AsyncWebServerRequest* request);

        // Reads the body, decodes it and, if valid, runs the command.
        template <typename Decoder>
        void HandleWrite(AsyncWebServerRequest* request, Decoder decode);

        void Submit(AsyncWebServerRequest* request, const Core::Command& command);
        void Reply(AsyncWebServerRequest* request, Core::ErrorCode result);

        // Every call is logged (tag "REST"): once when it arrives and once when it is answered.
        void LogReceived(AsyncWebServerRequest* request, const char* body);
        void Answer(AsyncWebServerRequest* request, int status, const char* body);

        AsyncWebServer& _server;
        CommandQueue& _commands;
        const SnapshotCache& _snapshot;
        Core::ILog& _log;
    };
}
