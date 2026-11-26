//
// Created by lmartinignacio@gmail.com on 4/11/2026.
//

#include <driver/A02YYUW_UART.h>

namespace CE::Driver
{
    A02YYUW_UART::A02YYUW_UART(const byte rxPin, const byte txPin, HardwareSerial& serial) : _rxPin(rxPin), _txPin(txPin),
        _serial(serial), _index(0), _buffer{}
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "_rxPin %d", _rxPin);
        ESP_LOGD(TAG, "_txPin %d", _txPin);
    }

    A02YYUW_UART::A02YYUW_UART(const byte rxPin, const byte txPin, HardwareSerial& serial, const char* debugName) : TAG(debugName),
        _rxPin(rxPin), _txPin(txPin), _serial(serial), _index(0), _buffer{}
    {
        ESP_LOGV(TAG, "Constructor");
        ESP_LOGD(TAG, "_rxPin %d", _rxPin);
        ESP_LOGD(TAG, "_txPin %d", _txPin);
    }

    bool A02YYUW_UART::Setup() const
    {
        ESP_LOGV(TAG, "Setup");

        ESP_LOGD(TAG, "_rxPin %d", _rxPin);
        ESP_LOGD(TAG, "_txPin %d", _txPin);

        _serial.begin(BAUD_RATE, CONFIG, _rxPin, _txPin);

        if (!_serial)
        {
            ESP_LOGE(TAG, "HardwareSerial is invalid!");
            return false;
        }

        return true;
    }

bool A02YYUW_UART::TryRead()
{
    //ESP_LOGV(TAG, "TryRead");

    while (_serial.available())
    {
        ESP_LOGV(TAG, "Serial Available");
        const uint8_t b = _serial.read();
        ESP_LOGD(TAG, "RX byte: 0x%02X", b);

        if (_index == 0)
        {
            if (b != 0xFF)
                continue;

            _buffer[_index++] = b;
            ESP_LOGV(TAG, "Found Header");
            continue;
        }

        _buffer[_index++] = b;
        ESP_LOGV(TAG, "Storing %d more bytes", 4 - _index);

        if (_index == 4)
        {
            _index = 0;

            ESP_LOGV(TAG, "Have 4 bytes. Verifying.");

            const uint8_t header = _buffer[0];
            const uint8_t high   = _buffer[1];
            const uint8_t low    = _buffer[2];
            const uint8_t check  = _buffer[3];

            const uint16_t sum = static_cast<uint16_t>(header) +
                                 static_cast<uint16_t>(high) +
                                 static_cast<uint16_t>(low);

            const uint8_t calc = static_cast<uint8_t>(sum & 0xFF);

            ESP_LOGD(TAG, "Frame: %02X %02X %02X %02X", header, high, low, check);
            ESP_LOGD(TAG, "Checksum calc=%02X check=%02X", calc, check);

            if (header == 0xFF && calc == check)
            {
                sample.distanceCm = ((high << 8) | low) * 100.0f;
                return true;
            }

            ESP_LOGW(TAG, "Invalid frame");
        }
    }

    return false;
}
}