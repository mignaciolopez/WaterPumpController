#include "core/Tank.h"

namespace CE::Core
{
    Tank::Tank(PumpController& pump, IClock& clock, const Settings& settings)
        : _pump(pump), _clock(clock), _settings(settings), _filter(settings.medianWindow)
    {
    }

    bool Tank::AddRule(const IRule& rule)
    {
        if (_ruleCount >= kMaxRules) return false;

        _rules[_ruleCount++] = &rule;
        return true;
    }

    void Tank::OnDistanceSample(const float distanceCm)
    {
        _filter.SetWindow(_settings.medianWindow);
        _filter.Add(distanceCm);
    }

    void Tank::OnDistanceFailure()
    {
        _filter.Reset();
    }

    float Tank::LevelCm() const
    {
        return HasLevel() ? LevelFromDistanceCm(_filter.Median(), _settings) : 0.0f;
    }

    void Tank::Update()
    {
        _pump.SetConfig(PumpConfigFrom(_settings));
        _filter.SetWindow(_settings.medianWindow);

        const bool timeValid = _clock.IsSynced();
        const RuleContext context{_settings, HasLevel(), LevelCm(), _pump.State().isOn, timeValid,
                                  timeValid ? _clock.LocalHour() : 0};

        bool veto = false, wantOn = false, wantOff = false;
        for (size_t i = 0; i < _ruleCount; ++i)
        {
            switch (_rules[i]->Evaluate(context))
            {
                case RuleAction::VetoOn:  veto = true;    break;
                case RuleAction::WantOn:  wantOn = true;  break;
                case RuleAction::WantOff: wantOff = true; break;
                case RuleAction::NoOpinion: break;
            }
        }

        _pump.Update(veto ? PumpController::AutoStart::Inhibit : PumpController::AutoStart::Allow);

        // Refusals (too soon, cooldown, ...) are fine: the rules ask again on the next Update.
        if (wantOff) (void)_pump.Request(false);
        else if (wantOn && !veto) (void)_pump.Request(true);
    }
}
