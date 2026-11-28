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

    /// Owns all pump safety rules. Call Begin() once, then Update() periodically.
    ///
    /// Rules:
    ///  - Stopping is always allowed. Starting is refused during fault, cooldown,
    ///    high water level, or within minRestartIntervalS of the last stop.
    ///  - High level and max on-time stop the pump immediately.
    ///  - No synced clock, or inconsistent level sensors, turn the pump off (Fault).
    ///    Faults clear on their own once the cause is gone. Cooldown survives faults.
    class PumpController
    {
    public:
        PumpController(ISwitch& relay, IFloatSwitch& lowSensor, IFloatSwitch& highSensor,
                       IClock& clock, IStore& store, ILog& log, PumpConfig config = {});

        /// Forces the relay off and restores persisted state.
        void Begin();

        /// Evaluates sensors and timers; starts/stops the pump as needed.
        void Update();

        /// Manual command. Returns why a start was refused.
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
