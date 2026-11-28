#pragma once

#include <esp_http_server.h>

#include <string>

#include "adapters/net/CommandQueue.h"
#include "adapters/net/HttpServer.h"
#include "adapters/net/SnapshotCache.h"
#include "core/Messages.h"

namespace CE::Adapters
{
    /// REST interface under /api/v1 (see docs/rest-api.md).
    ///
    ///   GET  /api/v1/state      latest state, from the cache (never blocks on the pump logic)
    ///   GET  /api/v1/settings   latest settings, from the cache
    ///   POST /api/v1/pump       {"on":true}   goes through the command queue
    ///   PUT  /api/v1/settings   {"minLevelCm":90,...}   goes through the command queue
    ///
    /// Handlers run in the HTTP server task. A write waits (up to 2 s) for the control task to run it, then
    /// answers with its result. Writes must have Content-Type: application/json, so a web page on another site
    /// cannot trigger them with a "simple" cross-site request. Every call is logged (tag "REST").
    class RestApi
    {
    public:
        RestApi(HttpServer& http, CommandQueue& commands, const SnapshotCache& snapshot);

        /// Registers the routes (the server must be started).
        [[nodiscard]] bool Begin();

    private:
        static constexpr uint32_t kWriteTimeoutMs = 2000;

        static esp_err_t OnState(httpd_req_t* req);
        static esp_err_t OnSettingsGet(httpd_req_t* req);
        static esp_err_t OnPump(httpd_req_t* req);
        static esp_err_t OnSettingsPut(httpd_req_t* req);

        // Reads the body, decodes it and, if valid, runs the command.
        template <typename Decoder>
        esp_err_t HandleWrite(httpd_req_t* req, Decoder decode);

        bool HasJsonContentType(httpd_req_t* req) const;
        esp_err_t Submit(httpd_req_t* req, const Core::Command& command);
        esp_err_t Reply(httpd_req_t* req, Core::ErrorCode result);

        HttpServer& _http;
        CommandQueue& _commands;
        const SnapshotCache& _snapshot;
    };
}
