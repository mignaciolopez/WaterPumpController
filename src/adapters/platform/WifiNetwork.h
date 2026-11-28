#pragma once

#include "core/ports/INetwork.h"

namespace CE::Adapters
{
    /// INetwork on top of the ESP32 WiFi station. Begin() does not block.
    /// The strings must outlive the object (use string literals / macros).
    class WifiNetwork final : public Core::INetwork
    {
    public:
        WifiNetwork(const char* ssid, const char* password);

        void Begin();

        bool IsConnected() const override;
        bool PingGateway() override;
        void Reconnect() override;

    private:
        const char* _ssid;
        const char* _password;
    };
}
