#pragma once

#include <cstdint>

#include "core/ports/IClock.h"
#include "core/ports/IFloatSwitch.h"
#include "core/ports/ILog.h"
#include "core/ports/IStore.h"
#include "core/ports/ISwitch.h"

namespace CE::Core
{
    enum class PumpMode : uint8_t
    {
        Idle,
        Running,
        Cooldown,
        Fault
    };

    enum class PumpFault : uint8_t
    {
        None,
        NoTime,             // clock is not synced
        SensorInconsistent  // high sensor wet while low sensor dry
    };

    enum class PumpResult : uint8_t
    {
        Ok,
        Fault,
        Cooldown,
        HighLevel,
        TooSoon
    };

    struct PumpConfig
    {
        uint32_t minRunTimeS         = 10;       // minimum time ON before a manual/level stop is accepted
        uint32_t minRestartIntervalS = 10;       // minimum time between OFF and the next ON
        uint32_t maxOnTimeS          = 20 * 60;  // longest continuous run before forced OFF + cooldown
        uint32_t cooldownS           = 60 * 60;  // lockout after a max on-time stop
    };

    struct PumpState
    {
        bool      isOn              = false;
        bool      isOnCooldown      = false;
        PumpFault fault             = PumpFault::None;
        int64_t   timeStampOn       = 0;
        int64_t   timeStampOff      = 0;
        int64_t   timeStampCooldown = 0;
    };

    /// Mode derived from a state snapshot (faults take priority, then running, then cooldown).
    inline PumpMode PumpModeOf(const PumpState& state)
    {
        if (state.fault != PumpFault::None) return PumpMode::Fault;
        if (state.isOn) return PumpMode::Running;
        if (state.isOnCooldown) return PumpMode::Cooldown;
        return PumpMode::Idle;
    }

    /// Owns all pump safety rules. Call Begin() once, then Update() periodically.
    ///
    /// Rules:
    ///  - No rapid cycling: a start is refused within minRestartIntervalS of the last stop,
    ///    and a normal stop request is refused within minRunTimeS of the last start.
    ///  - Starting is also refused during fault, cooldown or high water level.
    ///  - Safety stops (high level, max on-time, faults) always act immediately.
    ///  - No synced clock, or inconsistent level sensors, turn the pump off (Fault).
    ///    Faults clear on their own once the cause is gone. Cooldown survives faults.
    class PumpController
    {
    public:
        PumpController(ISwitch& relay, IFloatSwitch& lowSensor, IFloatSwitch& highSensor,
                       IClock& clock, IStore& store, ILog& log, PumpConfig config = {});

        /// Forces the relay off and restores persisted state.
        void Begin();

        /// Whether Update() may start the pump on its own when the low sensor is dry.
        enum class AutoStart : uint8_t { Allow, Inhibit };

        /// Evaluates sensors and timers; starts/stops the pump as needed.
        /// Safety stops are applied regardless of `autoStart`.
        void Update(AutoStart autoStart = AutoStart::Allow);

        /// Manual/level command. Returns why it was refused (TooSoon, Cooldown, ...).
        PumpResult Request(bool on);

        void SetConfig(const PumpConfig& config);

        const PumpState& State() const { return _state; }
        PumpMode Mode() const;

    private:
        void Start(int64_t now);
        void Stop(int64_t now);
        void Fail(PumpFault reason);
        void Persist();

    private:
        static constexpr const char* kTag = "PumpController";

        ISwitch& _relay;
        IFloatSwitch& _low;
        IFloatSwitch& _high;
        IClock& _clock;
        IStore& _store;
        ILog& _log;

        PumpConfig _config;
        PumpState _state;
    };
}
