#include <App.h>

void setup()
{
    if (!CE::App::Instance().Setup())
    {
        sleep(5);
        esp_restart();
    }
}

void loop()
{
    CE::App::Instance().Loop();
}