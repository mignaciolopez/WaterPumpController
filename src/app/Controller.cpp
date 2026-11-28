#include "app/Controller.h"

#include <cmath>
#include <cstdio>

#include "adapters/codec/CodecV1.h"

namespace CE::App
{
    using namespace Core;
    namespace Codec = Adapters::CodecV1;

    namespace
    {
        bool SamePump(const PumpState& a, const PumpState& b)
        {
            return a.isOn == b.isOn && a.isOnCooldown == b.isOnCooldown && a.fault == b.fault &&
                   a.timeStampOn == b.timeStampOn && a.timeStampOff == b.timeStampOff &&
                   a.timeStampCooldown == b.timeStampCooldown;
        }

        float Rounded(const float cm)
        {
            return std::round(cm * 10.0f) / 10.0f;  // same resolution as the protocol
        }
    }

    Controller::Controller(ISwitch& relay, IFloatSwitch& lowSensor, IFloatSwitch& highSensor, IClock& clock,
                           IStore& store, ILog& log, IMessageSink& sink)
        : _sink(sink), _store(store), _log(log), _xkcLow(lowSensor), _xkcHigh(highSensor),
          _pump(relay, lowSensor, highSensor, clock, store, log),
          _tank(_pump, clock, _settings),
          _handler(_pump, _settings, store)
    {
    }

    void Controller::Begin()
    {
        LoadSettings(_store, _settings);
        _pump.SetConfig(PumpConfigFrom(_settings));
        _pump.Begin();

        // Rules are stored by pointer; both outlive the tank. The list has room, so this cannot fail.
        (void)_tank.AddRule(_levelRule);
        (void)_tank.AddRule(_nightRule);
    }

    void Controller::OnDistance(const DistanceSample& sample)
    {
        if (!sample.valid)
        {
            _tank.OnDistanceFailure();
            return;
        }

        _tank.OnDistanceSample(sample.cm);

        if (_tank.HasLevel())
        {
            // The window is full: this sample produced a new median.
            char message[96];
            std::snprintf(message, sizeof(message), "New median: %.1f cm (water level %.1f cm)",
                          _tank.MedianDistanceCm(), _tank.LevelCm());
            _log.Log(LogLevel::Info, "A02YYUW", message);
        }
    }

    void Controller::OnCommand(const uint32_t clientId, const Command& command)
    {
        const CommandOutcome outcome = _handler.Handle(command);

        if (outcome.error != ErrorCode::None)
        {
            _sink.Send(clientId, Codec::EncodeError(command.id, outcome.error));
            return;
        }

        switch (command.type)
        {
            case CommandType::StateGet:
                _sink.Send(clientId, Codec::EncodeStateResponse(command.id, _pump.State(), _tank.HasLevel(),
                                                                _tank.LevelCm()));
                break;
            case CommandType::SettingsGet:
                _sink.Send(clientId, Codec::EncodeSettingsResponse(command.id, _settings));
                break;
            case CommandType::PumpSet:
            case CommandType::SettingsSet:
                _sink.Send(clientId, Codec::EncodeOk(command.id));
                break;
        }

        Announce(outcome);
    }

    CommandOutcome Controller::Execute(const Command& command)
    {
        const CommandOutcome outcome = _handler.Handle(command);
        Announce(outcome);
        return outcome;
    }

    void Controller::Announce(const CommandOutcome& outcome)
    {
        if (outcome.settingsChanged)
        {
            ++_revision;
            _sink.Broadcast(Codec::EncodeSettingsChangedEvent(_settings));
        }
        PublishChanges();
    }

    void Controller::Update()
    {
        LogXkcChanges();
        _tank.Update();
        PublishChanges();
    }

    void Controller::LogXkcChanges()
    {
        const bool low = _xkcLow.IsDetectingWater();
        const bool high = _xkcHigh.IsDetectingWater();

        if (!_xkcKnown || low != _lastXkcLow)
            _log.Log(LogLevel::Info, "XKC_Y23", low ? "Low: water detected" : "Low: no water");
        if (!_xkcKnown || high != _lastXkcHigh)
            _log.Log(LogLevel::Info, "XKC_Y23", high ? "High: water detected" : "High: no water");

        _xkcKnown = true;
        _lastXkcLow = low;
        _lastXkcHigh = high;
    }

    void Controller::PublishChanges()
    {
        const PumpState& pump = _pump.State();
        if (!_published || !SamePump(pump, _lastPump))
        {
            _lastPump = pump;
            ++_revision;
            _sink.Broadcast(Codec::EncodePumpStateEvent(pump));
        }

        const bool levelValid = _tank.HasLevel();
        const float levelCm = levelValid ? Rounded(_tank.LevelCm()) : 0.0f;
        if (!_published || levelValid != _lastLevelValid || levelCm != _lastLevelCm)
        {
            _lastLevelValid = levelValid;
            _lastLevelCm = levelCm;
            ++_revision;
            _sink.Broadcast(Codec::EncodeTankLevelEvent(levelValid, levelCm));
        }

        _published = true;
    }
}
