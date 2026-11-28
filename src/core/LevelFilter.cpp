#include "core/LevelFilter.h"

#include <algorithm>

namespace CE::Core
{
    LevelFilter::LevelFilter(const size_t window)
        : _window(std::clamp<size_t>(window, 1, kMaxWindow))
    {
    }

    void LevelFilter::SetWindow(const size_t window)
    {
        const size_t clamped = std::clamp<size_t>(window, 1, kMaxWindow);
        if (clamped == _window) return;

        _window = clamped;
        Reset();
    }

    void LevelFilter::Add(const float value)
    {
        _samples[_head] = value;
        _head = (_head + 1) % _window;
        if (_count < _window) ++_count;
    }

    void LevelFilter::Reset()
    {
        _count = 0;
        _head = 0;
    }

    float LevelFilter::Median() const
    {
        if (!Ready()) return 0.0f;

        float sorted[kMaxWindow];
        std::copy(_samples, _samples + _window, sorted);
        std::sort(sorted, sorted + _window);

        if (_window % 2 == 1) return sorted[_window / 2];
        return (sorted[_window / 2 - 1] + sorted[_window / 2]) / 2.0f;
    }
}
