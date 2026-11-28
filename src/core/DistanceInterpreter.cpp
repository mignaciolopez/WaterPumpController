#include "core/DistanceInterpreter.h"

#include <algorithm>

namespace CE::Core
{
    DistanceInterpreter::DistanceInterpreter(const float minCm, const float maxCm, const float marginCm)
        : _minCm(minCm), _maxCm(maxCm), _marginCm(marginCm)
    {
    }

    DistanceReading DistanceInterpreter::Interpret(const bool echo, const float cm)
    {
        if (echo)
        {
            if (cm <= _minCm + _marginCm)
            {
                _last = Last::NearMin;
                return {DistanceStatus::TooClose, std::max(cm, _minCm), false};
            }
            if (cm >= _maxCm - _marginCm)
            {
                _last = Last::NearMax;
                return {DistanceStatus::OutOfRange, std::min(cm, _maxCm), false};
            }

            _last = Last::Middle;
            return {DistanceStatus::Ok, cm, false};
        }

        switch (_last)
        {
            case Last::NearMin: return {DistanceStatus::TooClose, _minCm, true};     // stays so while the silence lasts
            case Last::NearMax: return {DistanceStatus::OutOfRange, _maxCm, true};
            default:
                _last = Last::Unknown;
                return {DistanceStatus::NoEcho, 0.0f, false};
        }
    }
}
