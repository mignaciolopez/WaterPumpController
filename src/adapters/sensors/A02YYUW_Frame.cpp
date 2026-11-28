#include "adapters/sensors/A02YYUW_Frame.h"

namespace CE::Adapters
{
    bool A02YYUW_FrameParser::Feed(const uint8_t byte, uint16_t& distanceMm)
    {
        if (_count == 0 && byte != kHeader) return false;  // wait for a header

        _buffer[_count++] = byte;
        if (_count < kFrameSize) return false;

        const uint8_t checksum = static_cast<uint8_t>(_buffer[0] + _buffer[1] + _buffer[2]);
        if (checksum == _buffer[3])
        {
            distanceMm = static_cast<uint16_t>((_buffer[1] << 8) | _buffer[2]);
            _count = 0;
            return true;
        }

        // Bad frame: keep whatever follows the next header byte, it may be the start of a real frame.
        uint8_t next = 1;
        while (next < kFrameSize && _buffer[next] != kHeader) ++next;

        _count = 0;
        for (; next < kFrameSize; ++next) _buffer[_count++] = _buffer[next];
        return false;
    }
}
