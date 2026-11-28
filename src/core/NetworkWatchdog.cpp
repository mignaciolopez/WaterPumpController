#include "core/NetworkWatchdog.h"

namespace CE::Core
{
    NetworkWatchdogConfig NetworkWatchdogConfigFrom(const Settings& settings)
    {
        NetworkWatchdogConfig config;
        config.reconnectsBeforeRestart = settings.maxReconnects;
        return config;
    }

    NetworkWatchdog::NetworkWatchdog(INetwork& network, IRestarter& restarter, ILog& log,
                                     const NetworkWatchdogConfig config)
        : _network(network), _restarter(restarter), _log(log), _config(config)
    {
    }

    bool NetworkWatchdog::Tick()
    {
        if (_network.IsConnected() && _network.PingGateway())
        {
            if (_failures > 0 || _reconnects > 0) _log.Log(LogLevel::Info, kTag, "Network recovered");
            _failures = 0;
            _reconnects = 0;
            return true;
        }

        ++_failures;
        _log.Log(LogLevel::Warn, kTag, "Network check failed");

        if (_failures >= _config.failuresBeforeReconnect)
        {
            _failures = 0;
            ++_reconnects;
            _log.Log(LogLevel::Warn, kTag, "Reconnecting");
            _network.Reconnect();

            if (_reconnects >= _config.reconnectsBeforeRestart)
            {
                _log.Log(LogLevel::Error, kTag, "Network did not recover, restarting device");
                _restarter.Restart();
            }
        }
        return false;
    }
}
