#pragma once

#include <cstdint>

#include "core/CommandHandler.h"
#include "core/Messages.h"
#include "core/PumpController.h"
#include "core/Settings.h"
#include "core/Tank.h"
#include "core/ports/IClock.h"
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
    };

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

        /// Executes a command and answers `clientId`.
        void OnCommand(uint32_t clientId, const Core::Command& command);

        /// Runs the pump and tank logic and broadcasts what changed.
        void Update();

        const Core::Settings& GetSettings() const { return _settings; }

    private:
        void PublishChanges();
        void LogXkcChanges();

        Core::Settings _settings;
        Core::IMessageSink& _sink;
        Core::IStore& _store;
        Core::ILog& _log;
        Core::IFloatSwitch& _xkcLow;
        Core::IFloatSwitch& _xkcHigh;

        Core::PumpController _pump;
        Core::Tank _tank;
        Core::LevelRule _levelRule;
        Core::NightRule _nightRule;
        Core::CommandHandler _handler;

        // What clients were last told, to publish only changes.
        bool _published = false;
        Core::PumpState _lastPump;
        bool _lastLevelValid = false;
        float _lastLevelCm = 0.0f;

        bool _xkcKnown = false;
        bool _lastXkcLow = false;
        bool _lastXkcHigh = false;
    };
}
