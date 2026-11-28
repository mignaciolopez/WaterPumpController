#include "core/Messages.h"

namespace CE::Core
{
    const char* ErrorCodeName(const ErrorCode code)
    {
        switch (code)
        {
            case ErrorCode::None:           return "";
            case ErrorCode::BadRequest:     return "BAD_REQUEST";
            case ErrorCode::UnknownCommand: return "UNKNOWN_COMMAND";
            case ErrorCode::UnknownField:   return "UNKNOWN_FIELD";
            case ErrorCode::InvalidValue:   return "INVALID_VALUE";
            case ErrorCode::PumpCooldown:   return "PUMP_COOLDOWN";
            case ErrorCode::PumpTooSoon:    return "PUMP_TOO_SOON";
            case ErrorCode::HighLevel:      return "HIGH_LEVEL";
            case ErrorCode::PumpFault:      return "PUMP_FAULT";
            case ErrorCode::Internal:       return "INTERNAL";
        }
        return "INTERNAL";
    }

    ErrorCode ToErrorCode(const PumpResult result)
    {
        switch (result)
        {
            case PumpResult::Ok:        return ErrorCode::None;
            case PumpResult::Fault:     return ErrorCode::PumpFault;
            case PumpResult::Cooldown:  return ErrorCode::PumpCooldown;
            case PumpResult::HighLevel: return ErrorCode::HighLevel;
            case PumpResult::TooSoon:   return ErrorCode::PumpTooSoon;
        }
        return ErrorCode::Internal;
    }

    ErrorCode ToErrorCode(const SettingResult result)
    {
        switch (result)
        {
            case SettingResult::Ok:           return ErrorCode::None;
            case SettingResult::UnknownField: return ErrorCode::UnknownField;
            case SettingResult::OutOfRange:   return ErrorCode::InvalidValue;
            case SettingResult::Inconsistent: return ErrorCode::InvalidValue;
        }
        return ErrorCode::Internal;
    }
}
