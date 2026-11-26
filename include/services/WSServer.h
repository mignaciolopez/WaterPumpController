//
// Created by lmartinignacio@gmail.com on 2/28/2026.
//

#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <map>
#include <functional>

namespace CE::Services
{
    class WSServer
    {
    public:
        using MessageHandler = std::function<bool(JsonDocument&)>;

        static WSServer& Instance();

        WSServer(const WSServer&) = delete;
        void operator=(const WSServer&) = delete;

        [[nodiscard]] bool Setup();
        void Loop();

        template<class T>
        void Broadcast(const char *header, const T& sample)
        {
            JsonDocument doc;
            doc["header"]   =   header;
            sample.toJson(doc.as<JsonVariant>(), "payload");

            String buffer;
            const size_t len = serializeJson(doc, buffer);
            _ws.textAll(buffer.c_str(), len);
        }

        size_t Subscribe(MessageHandler handler);
        void Unsubscribe(size_t id);

    private:
        static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
        void HandleWebSocketMessage(void *arg, uint8_t *data, size_t len);

    public:

    private:
        static WSServer instance;
        explicit WSServer(ushort port = 80, const char* url = "/ws");

        const char* TAG = "Service-WebSocket-Server";

        AsyncWebServer _server;
        AsyncWebSocket _ws;
        std::map<size_t, MessageHandler> _subscribers;
        size_t _nextSubscriberId = 0;
    };
}
