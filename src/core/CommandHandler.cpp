#include "core/CommandHandler.h"

namespace CE::Core
{
    CommandHandler::CommandHandler(PumpController& pump, Settings& settings, IStore& store)
        : _pump(pump), _settings(settings), _store(store)
    {
    }

    CommandOutcome CommandHandler::Handle(const Command& command)
    {
        switch (command.type)
        {
            case CommandType::PumpSet:
                return {ToErrorCode(_pump.Request(command.on)), false};

            case CommandType::StateGet:
            case CommandType::SettingsGet:
                return {};

            case CommandType::SettingsSet:
            {
                SettingUpdate updates[Command::kMaxSettingValues];
                const size_t count = command.valueCount < Command::kMaxSettingValues
                                         ? command.valueCount
                                         : Command::kMaxSettingValues;
                for (size_t i = 0; i < count; ++i) updates[i] = {command.values[i].name, command.values[i].value};

                const Settings previous = _settings;
                const SettingResult result = SetSettings(_settings, updates, count);
                if (result != SettingResult::Ok) return {ToErrorCode(result), false};

                if (!SaveSettings(_store, _settings))
                {
                    _settings = previous;   // not persisted: do not run with values that vanish on reboot
                    return {ErrorCode::Internal, false};
                }
                return {ErrorCode::None, true};
            }
        }
        return {ErrorCode::Internal, false};
    }
}
