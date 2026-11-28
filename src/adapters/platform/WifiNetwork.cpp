#include "adapters/platform/WifiNetwork.h"

#include <ESP32Ping.h>
#include <WiFi.h>

namespace CE::Adapters
{
    WifiNetwork::WifiNetwork(const char* ssid, const char* password) : _ssid(ssid), _password(password)
    {
    }

    void WifiNetwork::Begin()
    {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.begin(_ssid, _password);
    }

    bool WifiNetwork::IsConnected() const
    {
        return WiFi.status() == WL_CONNECTED;
    }

    bool WifiNetwork::PingGateway()
    {
        const IPAddress gateway = WiFi.gatewayIP();
        if (gateway == IPAddress(0, 0, 0, 0)) return false;

        return Ping.ping(gateway, 1);
    }

    void WifiNetwork::Reconnect()
    {
        WiFi.disconnect();
        WiFi.begin(_ssid, _password);
    }
}
