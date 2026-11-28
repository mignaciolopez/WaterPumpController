#pragma once

#include <cstdint>

#include "core/Settings.h"
#include "core/ports/ILog.h"
#include "core/ports/INetwork.h"
#include "core/ports/IRestarter.h"

namespace CE::Core
{
    struct NetworkWatchdogConfig
    {
        uint16_t failuresBeforeReconnect = 3;   // consecutive failed checks before reconnecting
        uint16_t reconnectsBeforeRestart = 10;  // reconnect rounds in a row before restarting the device
    };

    NetworkWatchdogConfig NetworkWatchdogConfigFrom(const Settings& settings);

    /// Keeps the device reachable. Call Tick() every Settings::watchdogDelayS seconds.
    ///
    /// A check passes when the link is up and the gateway answers a ping.
    /// Escalation: failed checks -> reconnect -> restart. Any passing check resets the counters.
    /// The pump logic does not depend on the network: it stays safe while the link is down.
    class NetworkWatchdog
    {
    public:
        NetworkWatchdog(INetwork& network, IRestarter& restarter, ILog& log, NetworkWatchdogConfig config = {});

        void SetConfig(const NetworkWatchdogConfig& config) { _config = config; }

        /// Performs one check. Returns true if the network is healthy.
        bool Tick();

    private:
        static constexpr const char* kTag = "NetworkWatchdog";

        INetwork& _network;
        IRestarter& _restarter;
        ILog& _log;
        NetworkWatchdogConfig _config;
        uint16_t _failures = 0;
        uint16_t _reconnects = 0;
    };
}
