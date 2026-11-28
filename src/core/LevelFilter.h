#pragma once

#include <cstddef>

namespace CE::Core
{
    /// Sliding-window median filter for noisy distance samples.
    /// The median is only available once the window has been filled (Ready()).
    class LevelFilter
    {
    public:
        static constexpr size_t kMaxWindow = 15;

        explicit LevelFilter(size_t window = 5);

        /// Changes the window size (clamped to 1..kMaxWindow). Resets the samples if it changed.
        void SetWindow(size_t window);

        void Add(float value);
        void Reset();

        bool Ready() const { return _count == _window; }

        /// Median of the window. Only valid when Ready(); returns 0 otherwise.
        float Median() const;

    private:
        float _samples[kMaxWindow] = {};
        size_t _window;
        size_t _count = 0;
        size_t _head = 0;
    };
}
