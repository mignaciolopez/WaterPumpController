#include "adapters/sensors/A02YYUW_UART.h"

namespace CE::Adapters
{
    A02YYUW_UART::A02YYUW_UART(const byte rxPin, const byte txPin, HardwareSerial& serial)
        : _rxPin(rxPin), _txPin(txPin), _serial(serial)
    {
    }

    bool A02YYUW_UART::Begin()
    {
        _serial.begin(kBaudRate, SERIAL_8N1, _rxPin, _txPin);
        _parser.Reset();
        return static_cast<bool>(_serial);
    }

    Core::DistanceReading A02YYUW_UART::Read()
    {
        bool gotFrame = false;
        uint16_t latestMm = 0;

        while (_serial.available())
        {
            uint16_t mm = 0;
            if (_parser.Feed(static_cast<uint8_t>(_serial.read()), mm))
            {
                latestMm = mm;  // keep the newest frame
                gotFrame = true;
            }
        }

        if (!gotFrame) return {Core::DistanceStatus::NoEcho, 0.0f};

        const float cm = static_cast<float>(latestMm) / 10.0f;  // the sensor reports millimetres
        return Core::ClassifyDistance(cm, kMinDistanceCm, kMaxDistanceCm);
    }
}
