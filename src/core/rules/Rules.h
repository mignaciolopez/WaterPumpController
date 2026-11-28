#pragma once

#include "core/Settings.h"

namespace CE::Core
{
    /// Inputs a rule may look at. `level*` is only valid when `hasLevel` is true.
    struct RuleContext
    {
        const Settings& settings;
        bool  hasLevel  = false;
        float levelCm   = 0.0f;
        bool  pumpOn    = false;
        bool  timeValid = false;
        int   localHour = 0;
    };

    enum class RuleAction : uint8_t
    {
        NoOpinion,
        WantOn,
        WantOff,
        VetoOn   // the pump must not be started (by anyone) while this holds
    };

    /// A pump decision rule. Rules are stateless; the Tank combines their answers:
    /// any VetoOn blocks starting, any WantOff requests a stop, else WantOn requests a start.
    /// Safety limits (max on-time, high level, faults) live in PumpController, not in rules.
    class IRule
    {
    public:
        virtual ~IRule() = default;

        virtual RuleAction Evaluate(const RuleContext& context) const = 0;
    };

    /// True when `hour` lies in the night window. The window may span midnight (e.g. 22 -> 8).
    bool IsNightHour(int hour, int startHour, int endHour);

    /// Start at or below minLevelCm, stop at or above maxLevelCm.
    class LevelRule final : public IRule
    {
    public:
        RuleAction Evaluate(const RuleContext& context) const override;
    };

    /// At night, vetoes starting unless the level is at or below criticalLevelCm.
    /// Without a level reading or a valid clock it has no opinion (the pump is not blocked).
    class NightRule final : public IRule
    {
    public:
        RuleAction Evaluate(const RuleContext& context) const override;
    };
}
