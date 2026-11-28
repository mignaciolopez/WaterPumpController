#pragma once

#include <cstdint>

namespace CE::Adapters
{
    /// Parser for the A02YYUW UART output: 4-byte frames 0xFF, high, low, checksum.
    /// distance (mm) = high * 256 + low, checksum = (0xFF + high + low) & 0xFF.
    /// Plain C++ (no Arduino includes) so it can be tested on the PC.
    class A02YYUW_FrameParser
    {
    public:
        /// Feeds one received byte. Returns true when it completes a valid frame;
        /// `distanceMm` is then set. Garbage and bad checksums are skipped, and the parser
        /// re-synchronizes on the next 0xFF header.
        bool Feed(uint8_t byte, uint16_t& distanceMm);

        void Reset() { _count = 0; }

    private:
        static constexpr uint8_t kHeader = 0xFF;
        static constexpr uint8_t kFrameSize = 4;

        uint8_t _buffer[kFrameSize] = {};
        uint8_t _count = 0;
    };
}
