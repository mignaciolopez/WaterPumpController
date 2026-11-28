#pragma once

#include <esp_system.h>

#include "core/ports/IRestarter.h"

namespace CE::Adapters
{
    class EspRestarter final : public Core::IRestarter
    {
    public:
        void Restart() override { esp_restart(); }
    };
}
