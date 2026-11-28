#include <Arduino.h>

#include "app/Firmware.h"
#include "app/profiles/EspWroverKit.h"

// Created in setup(), not at static-initialization time, so the FreeRTOS objects inside exist
// only after the system is up. It lives for the whole run of the program.
static CE::App::Firmware* firmware = nullptr;

void setup()
{
    firmware = new CE::App::Firmware(CE::App::kEspWroverKit);

    if (!firmware->Begin())
    {
        delay(5000);
        ESP.restart();
    }
}

void loop()
{
    // All the work happens in the firmware's tasks.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
