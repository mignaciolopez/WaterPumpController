#pragma once

#include <cstdint>
#include <string>

namespace CE::Core
{
    /// Where outgoing protocol messages go (WebSocket today, MQTT later).
    class IMessageSink
    {
    public:
        virtual ~IMessageSink() = default;

        /// Sends a message to one client (responses).
        virtual void Send(uint32_t clientId, const std::string& message) = 0;

        /// Sends a message to every connected client (events).
        virtual void Broadcast(const std::string& message) = 0;
    };
}
