#include "adapters/net/RestApi.h"

#include <strings.h>

#include "adapters/codec/RestCodec.h"

namespace CE::Adapters
{
    namespace
    {
        RestApi* Self(httpd_req_t* req)
        {
            return static_cast<RestApi*>(req->user_ctx);
        }
    }

    RestApi::RestApi(HttpServer& http, CommandQueue& commands, const SnapshotCache& snapshot)
        : _http(http), _commands(commands), _snapshot(snapshot)
    {
    }

    bool RestApi::Begin()
    {
        const httpd_uri_t routes[] = {
            {"/api/v1/state",    HTTP_GET,  &RestApi::OnState,       this},
            {"/api/v1/settings", HTTP_GET,  &RestApi::OnSettingsGet, this},
            {"/api/v1/pump",     HTTP_POST, &RestApi::OnPump,        this},
            {"/api/v1/settings", HTTP_PUT,  &RestApi::OnSettingsPut, this},
        };

        for (const httpd_uri_t& route : routes)
            if (httpd_register_uri_handler(_http.Handle(), &route) != ESP_OK) return false;

        return true;
    }

    esp_err_t RestApi::OnState(httpd_req_t* req)
    {
        RestApi* self = Self(req);
        self->_http.LogRequest(req, nullptr);
        return self->_http.Respond(req, 200, self->_snapshot.State());
    }

    esp_err_t RestApi::OnSettingsGet(httpd_req_t* req)
    {
        RestApi* self = Self(req);
        self->_http.LogRequest(req, nullptr);
        return self->_http.Respond(req, 200, self->_snapshot.Settings());
    }

    esp_err_t RestApi::OnPump(httpd_req_t* req)
    {
        return Self(req)->HandleWrite(req, RestCodec::DecodePumpBody);
    }

    esp_err_t RestApi::OnSettingsPut(httpd_req_t* req)
    {
        return Self(req)->HandleWrite(req, RestCodec::DecodeSettingsBody);
    }

    bool RestApi::HasJsonContentType(httpd_req_t* req) const
    {
        char type[64];
        if (httpd_req_get_hdr_value_str(req, "Content-Type", type, sizeof(type)) != ESP_OK) return false;
        return strncasecmp(type, "application/json", 16) == 0;
    }

    template <typename Decoder>
    esp_err_t RestApi::HandleWrite(httpd_req_t* req, Decoder decode)
    {
        const size_t total = req->content_len;
        std::string body;
        bool bodyRead = false;

        if (total > 0 && total <= RestCodec::kMaxBodyBytes)
        {
            body.resize(total);
            size_t received = 0;
            while (received < total)
            {
                const int n = httpd_req_recv(req, &body[received], total - received);
                if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;  // try again
                if (n <= 0) break;
                received += static_cast<size_t>(n);
            }
            bodyRead = received == total;
        }

        _http.LogRequest(req, bodyRead ? body.c_str() : nullptr);

        if (!HasJsonContentType(req))
            return _http.Respond(req, 415, RestCodec::EncodeError(Core::ErrorCode::BadRequest));

        if (!bodyRead)
        {
            // Too large or missing: an unread body would be taken for the next request, so close the connection.
            httpd_resp_set_hdr(req, "Connection", "close");
            return Reply(req, Core::ErrorCode::BadRequest);
        }

        Core::Command command;
        const Core::ErrorCode error = decode(body.c_str(), body.size(), command);
        if (error != Core::ErrorCode::None) return Reply(req, error);

        return Submit(req, command);
    }

    esp_err_t RestApi::Submit(httpd_req_t* req, const Core::Command& command)
    {
        RestTicket* ticket = RestTicket::Create();
        if (ticket == nullptr) return Reply(req, Core::ErrorCode::Internal);

        Incoming incoming;
        incoming.command = command;
        incoming.ticket = ticket;

        if (!_commands.Push(incoming))
        {
            ticket->Release();  // the control task will never see it: drop both references
            ticket->Release();
            return Reply(req, Core::ErrorCode::Internal);
        }

        const Core::ErrorCode result = ticket->Wait(kWriteTimeoutMs);
        ticket->Release();
        return Reply(req, result);
    }

    esp_err_t RestApi::Reply(httpd_req_t* req, const Core::ErrorCode result)
    {
        const std::string body = result == Core::ErrorCode::None ? RestCodec::EncodeOk() : RestCodec::EncodeError(result);
        return _http.Respond(req, RestCodec::HttpStatus(result), body);
    }
}
