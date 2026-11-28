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
        : _sink(sink), _store(store), _log(log), _clock(clock), _xkcLow(lowSensor), _xkcHigh(highSensor),
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
            RecordMedian(_tank.MedianDistanceCm());

            const int64_t now = _clock.NowS();
            if (!_medianLogged || now - _lastMedianLogAt >= kMedianLogIntervalS) LogMedian(now);
        }
    }

    void Controller::RecordMedian(const float distanceCm)
    {
        _medians[_medianNext] = distanceCm;
        _medianNext = (_medianNext + 1) % kMedianHistory;
        if (_medianCount < kMedianHistory) ++_medianCount;
    }

    void Controller::LogMedian(const int64_t now)
    {
        _medianLogged = true;
        _lastMedianLogAt = now;

        // "New median: 90.0 cm (water level 50.0 cm) (88, 89, 90 cm)": the list is oldest first, whole
        // numbers, and only the last one carries the unit.
        char message[224];
        int length = std::snprintf(message, sizeof(message), "New median: %.1f cm (water level %.1f cm) (",
                                   _tank.MedianDistanceCm(), _tank.LevelCm());

        const size_t first = (_medianNext + kMedianHistory - _medianCount) % kMedianHistory;
        for (size_t i = 0; i < _medianCount && length > 0 && static_cast<size_t>(length) < sizeof(message); ++i)
        {
            const bool last = i + 1 == _medianCount;
            const long value = std::lround(_medians[(first + i) % kMedianHistory]);
            length += std::snprintf(message + length, sizeof(message) - length, last ? "%ld cm)" : "%ld, ", value);
        }

        _log.Log(LogLevel::Info, "A02YYUW", message);
    }

    void Controller::OnCommand(const uint32_t clientId, const Command& command)
    {
        const CommandOutcome outcome = _handler.Handle(command);
        LogFailure(outcome);

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
        LogFailure(outcome);
        Announce(outcome);
        return outcome;
    }

    void Controller::LogFailure(const CommandOutcome& outcome)
    {
        if (outcome.error == ErrorCode::Internal)
            _log.Log(LogLevel::Error, "Controller", "Command failed: the settings could not be saved");
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
