#pragma once

#include <cstddef>

#include "core/LevelFilter.h"
#include "core/PumpController.h"
#include "core/Settings.h"
#include "core/ports/IClock.h"
#include "core/rules/Rules.h"

namespace CE::Core
{
    /// One tank: a distance sensor (via its samples), a pump and the rules deciding when to pump.
    /// Call OnDistanceSample/OnDistanceFailure as readings arrive, and Update() periodically.
    class Tank
    {
    public:
        static constexpr size_t kMaxRules = 8;

        Tank(PumpController& pump, IClock& clock, const Settings& settings);

        /// Adds a rule. Returns false if the rule list is full. The rule must outlive the Tank.
        [[nodiscard]] bool AddRule(const IRule& rule);

        /// A valid distance reading (cm from the sensor down to the water).
        void OnDistanceSample(float distanceCm);

        /// A failed reading: the level becomes unknown until the window refills.
        void OnDistanceFailure();

        void Update();

        /// Float switches are the primary control: by default rules can veto starts and request
        /// stops, but a rule's WantOn is ignored. Enable to let rules start the pump too.
        void SetRulesMayStart(const bool enabled) { _rulesMayStart = enabled; }

        bool HasLevel() const { return _filter.Ready(); }
        float LevelCm() const;

    private:
        PumpController& _pump;
        IClock& _clock;
        const Settings& _settings;
        LevelFilter _filter;
        const IRule* _rules[kMaxRules] = {};
        size_t _ruleCount = 0;
        bool _rulesMayStart = false;
    };
}
