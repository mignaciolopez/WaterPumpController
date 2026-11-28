#include "core/rules/Rules.h"

namespace CE::Core
{
    bool IsNightHour(const int hour, const int startHour, const int endHour)
    {
        if (startHour == endHour) return false;
        if (startHour > endHour) return hour >= startHour || hour < endHour;
        return hour >= startHour && hour < endHour;
    }

    RuleAction LevelRule::Evaluate(const RuleContext& context) const
    {
        if (!context.hasLevel) return RuleAction::NoOpinion;

        if (context.pumpOn)
            return context.levelCm >= context.settings.maxLevelCm ? RuleAction::WantOff : RuleAction::NoOpinion;

        return context.levelCm <= context.settings.minLevelCm ? RuleAction::WantOn : RuleAction::NoOpinion;
    }

    RuleAction NightRule::Evaluate(const RuleContext& context) const
    {
        if (!context.hasLevel || !context.timeValid) return RuleAction::NoOpinion;

        const auto& s = context.settings;
        if (IsNightHour(context.localHour, s.nightStartHour, s.nightEndHour) && context.levelCm > s.criticalLevelCm)
            return RuleAction::VetoOn;

        return RuleAction::NoOpinion;
    }
}
