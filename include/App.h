//
// Created by lmartinignacio@gmail.com on 4/10/2026.
//

#pragma once

#include <Arduino.h>
#include <services/Pump.h>
#include <services/Radar.h>

namespace CE
{
    class App
    {
    public:
        static App& Instance();

        App(const App&) = delete;
        void operator=(const App&) = delete;

        [[nodiscard]] bool Setup();

        static void Loop();

        bool isRunning;

    private:
        explicit App();

    private:
        static App instance;
        const char* TAG = "Application";

        Services::Pump _pump;
        Services::Radar _radar;
    };
} // CE
