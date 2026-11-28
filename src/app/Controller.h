#pragma once

#include <cstdint>

#include "core/CommandHandler.h"
#include "core/Messages.h"
#include "core/PumpController.h"
#include "core/Settings.h"
#include "core/Tank.h"
#include "core/ports/IClock.h"
#include "core/ports/IDistanceSensor.h"
#include "core/ports/IFloatSwitch.h"
#include "core/ports/ILog.h"
#include "core/ports/IMessageSink.h"
#include "core/ports/IStore.h"
#include "core/ports/ISwitch.h"
#include "core/rules/Rules.h"

namespace CE::App
{
    /// A distance reading result from the sensor task.
    struct DistanceSample
    {
        bool  valid = false;
        float cm = 0.0f;

        /// The sensor said nothing and nothing explains it. With `blindZoneCm` set, the Controller may read it
        /// as "the water is closer than the sensor can measure" when the high XKC_Y23 sensor sees water.
        bool  silent = false;
        float blindZoneCm = 0.0f;  // the sensor's minimum range

        /// A valid sample that was inferred from the last readings before the sensor went silent (water that
        /// disappeared into the blind zone). Only believed while the low XKC_Y23 sensor sees water.
        bool  inferred = false;
    };

    /// Turns what a sensor reported into a sample for the Controller.
    inline DistanceSample ToSample(const Core::DistanceReading& reading, const float minDistanceCm)
    {
        DistanceSample sample;
        switch (reading.status)
        {
            case Core::DistanceStatus::Ok:
            case Core::DistanceStatus::TooClose:   // a real reading near the sensor's minimum, or inferred from one
                sample.valid = true;
                sample.cm = reading.cm;
                sample.inferred = reading.inferred;
                break;
            case Core::DistanceStatus::NoEcho:
                sample.silent = true;
                sample.blindZoneCm = minDistanceCm;
                break;
            case Core::DistanceStatus::OutOfRange:
                break;  // beyond the far limit: not a usable reading for a tank
        }
        return sample;
    }

    /// The application logic of one tank with one pump: wires the core pieces together and turns
    /// commands and state changes into protocol messages. Free of hardware and OS calls.
    ///
    /// Not thread-safe: everything must be called from one task (the control task).
    class Controller
    {
    public:
        Controller(Core::ISwitch& relay, Core::IFloatSwitch& lowSensor, Core::IFloatSwitch& highSensor,
                   Core::IClock& clock, Core::IStore& store, Core::ILog& log, Core::IMessageSink& sink);

        /// Loads the settings, forces the pump off and restores its state.
        void Begin();

        void OnDistance(const DistanceSample& sample);

        /// Executes a command and answers `clientId` with a WebSocket protocol response.
        void OnCommand(uint32_t clientId, const Core::Command& command);

        /// Executes a command and returns the outcome; the caller builds the reply (REST).
        /// Events are still broadcast to WebSocket clients.
        Core::CommandOutcome Execute(const Core::Command& command);

        /// Runs the pump and tank logic and broadcasts what changed.
        void Update();

        const Core::Settings& GetSettings() const { return _settings; }
        const Core::PumpState& GetPumpState() const { return _pump.State(); }
        bool HasLevel() const { return _tank.HasLevel(); }
        float LevelCm() const { return _tank.LevelCm(); }

        /// Increases whenever the pump state, the level or the settings changed. Lets other tasks know
        /// when a cached copy of the state is out of date.
        uint32_t Revision() const { return _revision; }

    private:
        void PublishChanges();
        void LogXkcChanges();
        void LogCooldown();
        void RecordMedian(float distanceCm);
        void LogMedian(int64_t now);
        void Announce(const Core::CommandOutcome& outcome);
        void LogFailure(const Core::CommandOutcome& outcome);

        Core::Settings _settings;
        Core::IMessageSink& _sink;
        Core::IStore& _store;
        Core::ILog& _log;
        Core::IClock& _clock;
        Core::IFloatSwitch& _xkcLow;
        Core::IFloatSwitch& _xkcHigh;

        Core::PumpController _pump;
        Core::Tank _tank;
        Core::LevelRule _levelRule;
        Core::NightRule _nightRule;
        Core::CommandHandler _handler;

        // What clients were last told, to publish only changes.
        bool _published = false;
        uint32_t _revision = 0;
        Core::PumpState _lastPump;
        bool _lastLevelValid = false;
        float _lastLevelCm = 0.0f;

        // Distance medians: logged at most every kMedianLogIntervalS, together with the last kMedianHistory.
        static constexpr int64_t kMedianLogIntervalS = 10;
        static constexpr size_t kMedianHistory = 15;
        float _medians[kMedianHistory] = {};
        size_t _medianNext = 0;      // where the next median is stored
        size_t _medianCount = 0;     // how many are stored (up to kMedianHistory)
        bool _medianLogged = false;
        int64_t _lastMedianLogAt = 0;

        // While the pump is on cooldown: logged right away and then every kCooldownLogIntervalS.
        static constexpr int64_t kCooldownLogIntervalS = 10;
        bool _cooldownLogged = false;
        int64_t _lastCooldownLogAt = 0;

        bool _blindZoneAssumed = false;  // logged once per episode

        bool _xkcKnown = false;
        bool _lastXkcLow = false;
        bool _lastXkcHigh = false;
    };
}
