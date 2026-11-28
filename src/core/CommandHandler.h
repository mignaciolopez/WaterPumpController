#pragma once

#include "core/Messages.h"
#include "core/PumpController.h"
#include "core/Settings.h"
#include "core/ports/IStore.h"

namespace CE::Core
{
    struct CommandOutcome
    {
        ErrorCode error = ErrorCode::None;
        bool settingsChanged = false;   // true when settings were applied and saved
    };

    /// Executes decoded commands against the pump and the settings.
    /// Not thread-safe: call it from the control task only.
    ///
    /// StateGet and SettingsGet change nothing; the caller builds the response from the
    /// current state (pump.State(), tank level, settings).
    class CommandHandler
    {
    public:
        CommandHandler(PumpController& pump, Settings& settings, IStore& store);

        CommandOutcome Handle(const Command& command);

    private:
        PumpController& _pump;
        Settings& _settings;
        IStore& _store;
    };
}
