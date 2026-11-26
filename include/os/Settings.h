//
// Created by lmartinignacio@gmail.com on 2/27/2026.
//

#pragma once

#include <domain/Settings.hpp>

namespace CE::OS
{
    class Settings
    {
    public:
        static const char* TAG;

        Settings();
        [[nodiscard]] static bool Setup();

        [[nodiscard]] static Domain::Settings& Get();

        [[nodiscard]] static bool Load();
        [[nodiscard]] static bool Save();

        static bool HandleWebSocketMessage(JsonDocument &doc);

    private:
        static Domain::Settings* _settings;
        static size_t _socketId;
    };

}   // namespace CE::Services