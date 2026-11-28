#include "core/PumpController.h"

namespace CE::Core
{
    namespace
    {
        constexpr const char* kKeyIsOn     = "pump.isOn";
        constexpr const char* kKeyCooldown = "pump.cd";
        constexpr const char* kKeyOn       = "pump.tOn";
        constexpr const char* kKeyOff      = "pump.tOff";
        constexpr const char* kKeyCdAt     = "pump.tCd";
    }

    PumpController::PumpController(ISwitch& relay, IFloatSwitch& lowSensor, IFloatSwitch& highSensor,
                                   IClock& clock, IStore& store, ILog& log, const PumpConfig config)
        : _relay(relay), _low(lowSensor), _high(highSensor), _clock(clock), _store(store), _log(log),
          _config(config)
    {
    }

    void PumpController::Begin()
    {
        // Fail safe: whatever the relay was doing before boot, it is off now.
        _relay.Set(false);

        int64_t isOn = 0, cooldown = 0, tOn = 0, tOff = 0, tCd = 0;
        const bool restored = _store.GetI64(kKeyIsOn, isOn) && _store.GetI64(kKeyCooldown, cooldown) &&
                              _store.GetI64(kKeyOn, tOn) && _store.GetI64(kKeyOff, tOff) &&
                              _store.GetI64(kKeyCdAt, tCd);

        if (!restored)
        {
            _log.Log(LogLevel::Warn, kTag, "No stored state, using defaults");
            _state = {};
            Persist();
            return;
        }

        _state.isOn = false;
        _state.isOnCooldown = cooldown != 0;
        _state.timeStampOn = tOn;
        _state.timeStampOff = tOff;
        _state.timeStampCooldown = tCd;

        if (isOn != 0)
        {
            // The pump was running when power was lost: it counts as stopped now.
            if (_clock.IsSynced()) _state.timeStampOff = _clock.NowS();
            Persist();
        }
    }

    PumpMode PumpController::Mode() const
    {
        return PumpModeOf(_state);
    }

    void PumpController::SetConfig(const PumpConfig& config)
    {
        _config = config;
    }

    void PumpController::Update(const AutoStart autoStart)
    {
        if (!_clock.IsSynced())
        {
            Fail(PumpFault::NoTime);
            return;
        }

        const bool lowWet = _low.IsDetectingWater();
        const bool highWet = _high.IsDetectingWater();

        if (highWet && !lowWet)
        {
            Fail(PumpFault::SensorInconsistent);
            return;
        }

        if (_state.fault != PumpFault::None)
        {
            _log.Log(LogLevel::Info, kTag, "Fault cleared");
            _state.fault = PumpFault::None;
        }

        const int64_t now = _clock.NowS();

        if (_state.isOnCooldown && now - _state.timeStampCooldown >= static_cast<int64_t>(_config.cooldownS))
        {
            _log.Log(LogLevel::Info, kTag, "Cooldown finished");
            _state.isOnCooldown = false;
            Persist();
        }

        if (_state.isOn)
        {
            if (highWet)
            {
                _log.Log(LogLevel::Info, kTag, "High level reached, stopping");
                Stop(now);
            }
            else if (now - _state.timeStampOn >= static_cast<int64_t>(_config.maxOnTimeS))
            {
                _log.Log(LogLevel::Warn, kTag, "Max on-time reached, stopping and cooling down");
                _state.isOnCooldown = true;
                _state.timeStampCooldown = now;
                Stop(now);
            }
        }
        else if (autoStart == AutoStart::Allow && !_state.isOnCooldown && !lowWet)
        {
            (void)Request(true); // may be refused (e.g. too soon); retried on the next Update
        }
    }

    PumpResult PumpController::Request(const bool on)
    {
        if (!on)
        {
            if (!_state.isOn) return PumpResult::Ok;

            const int64_t now = _clock.NowS();
            if (now - _state.timeStampOn < static_cast<int64_t>(_config.minRunTimeS)) return PumpResult::TooSoon;

            Stop(now);
            return PumpResult::Ok;
        }

        if (_state.fault != PumpFault::None || !_clock.IsSynced()) return PumpResult::Fault;
        if (_state.isOnCooldown) return PumpResult::Cooldown;
        if (_high.IsDetectingWater()) return PumpResult::HighLevel;
        if (_state.isOn) return PumpResult::Ok;

        const int64_t now = _clock.NowS();
        if (_state.timeStampOff != 0 && now - _state.timeStampOff < static_cast<int64_t>(_config.minRestartIntervalS))
            return PumpResult::TooSoon;

        Start(now);
        return PumpResult::Ok;
    }

    void PumpController::Start(const int64_t now)
    {
        _relay.Set(true);
        _state.isOn = true;
        _state.timeStampOn = now;
        _log.Log(LogLevel::Info, kTag, "Pump ON");
        Persist();
    }

    void PumpController::Stop(const int64_t now)
    {
        _relay.Set(false);
        _state.isOn = false;
        _state.timeStampOff = now;
        _log.Log(LogLevel::Info, kTag, "Pump OFF");
        Persist();
    }

    void PumpController::Fail(const PumpFault reason)
    {
        _relay.Set(false);

        const bool wasOn = _state.isOn;
        if (wasOn)
        {
            _state.isOn = false;
            if (_clock.IsSynced()) _state.timeStampOff = _clock.NowS();
        }

        if (_state.fault != reason)
        {
            _state.fault = reason;
            _log.Log(LogLevel::Error, kTag, reason == PumpFault::NoTime ? "Fault: clock not synced"
                                                                         : "Fault: inconsistent level sensors");
        }

        if (wasOn) Persist();
    }

    void PumpController::Persist()
    {
        const bool ok = _store.SetI64(kKeyIsOn, _state.isOn ? 1 : 0) &&
                        _store.SetI64(kKeyCooldown, _state.isOnCooldown ? 1 : 0) &&
                        _store.SetI64(kKeyOn, _state.timeStampOn) &&
                        _store.SetI64(kKeyOff, _state.timeStampOff) &&
                        _store.SetI64(kKeyCdAt, _state.timeStampCooldown);

        if (!ok) _log.Log(LogLevel::Error, kTag, "Failed to persist state");
    }
}
