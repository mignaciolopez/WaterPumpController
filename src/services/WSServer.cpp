//
// Created by lmartinignacio@gmail.com on 2/28/2026.
//

#include <ArduinoJson.h>
#include <services/WSServer.h>

namespace CE::Services
{
    WSServer WSServer::instance{};

    WSServer& WSServer::Instance()
    {
        return instance;
    }

    WSServer::WSServer(const ushort port, const char* url) : _server(port), _ws(url)
    {
        ESP_LOGV(TAG, "Constructor");
    }

    bool WSServer::Setup()
    {
        ESP_LOGV(TAG, "Setup");

        _ws.onEvent(onWsEvent);
        _server.addHandler(&_ws);
        
        // Optional: Add HTTP endpoint for status
        _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
        {
            request->send(200, "text/plain", "ESP32 Water Tank WebSocket Server");
        });
        
        _server.begin();
        ESP_LOGI(TAG, "WebSocket server started on port 80");

        return true;
    }
    
    void WSServer::Loop()
    {
        _ws.cleanupClients(); // Clean up disconnected clients
    }

    size_t WSServer::Subscribe(MessageHandler handler)
    {
        ESP_LOGD(TAG, "Subscribe. ID: %u", _nextSubscriberId);
        _subscribers.emplace(_nextSubscriberId, std::move(handler));
        return _nextSubscriberId++;
    }

    void WSServer::Unsubscribe(size_t id)
    {
        ESP_LOGD(TAG, "Unsubscribe. ID: %u", id);
        _subscribers.erase(id);
    }

    void WSServer::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, const AwsEventType type, void *arg, uint8_t *data, const size_t len)
    {
        switch(type)
        {
            case WS_EVT_CONNECT:
                ESP_LOGI(Instance().TAG, "Client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
                break;
            case WS_EVT_DISCONNECT:
                ESP_LOGI(Instance().TAG, "Client #%u disconnected", client->id());
                break;
            case WS_EVT_DATA:
                ESP_LOGD(Instance().TAG, "Message: WS_EVT_DATA. Client IP: %s", client->remoteIP().toString().c_str());
                Instance().HandleWebSocketMessage(arg, data, len);
                break;
            default:
                ESP_LOGD(Instance().TAG, "Message: %d Not Handled. Client IP: %s", type, client->remoteIP().toString().c_str());
                break;
        }
    }

    void WSServer::HandleWebSocketMessage(void *arg, uint8_t *data, const size_t len)
    {
        ESP_LOGV(TAG, "handleWebSocketMessage");
        const auto info = static_cast<AwsFrameInfo*>(arg);
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
        {
            data[len] = 0; // Null terminate

            JsonDocument doc;
            const DeserializationError error = deserializeJson(doc, reinterpret_cast<char *>(data));

            if (error)
            {
                ESP_LOGE(TAG, "JSON parse error: %s", error.c_str());
                return;
            }

            const char* command = doc["command"];

            ESP_LOGD(TAG, "command: %s", command);

            for (const auto& [id, subscriber] : _subscribers)
            {
                if (subscriber(doc))
                    break;
            }
        }
    }
}
