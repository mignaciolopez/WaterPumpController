#pragma once

namespace CE::Core
{
    /// Network link used by the watchdog.
    class INetwork
    {
    public:
        virtual ~INetwork() = default;

        /// True if the link layer (WiFi) reports a connection.
        virtual bool IsConnected() const = 0;

        /// True if the router/gateway answers a ping.
        virtual bool PingGateway() = 0;

        /// Drops and re-establishes the connection.
        virtual void Reconnect() = 0;
    };
}
