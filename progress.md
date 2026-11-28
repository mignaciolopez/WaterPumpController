# Implementation Progress
Branch: `refactor/ports-and-adapters` · Plan: see "Migration Plan" in `constitution.md`

## Summary
- **Steps Completed:** 6 / 6 (steps 5 and 6 done together)
- **Current Step:** Done - flash to the board and verify on hardware (see Hardware checklist)
- **Checks (last run):** `pio test -e native` all passing · `pio run -e esp-wrover-kit` builds

## Task Log
### Step 1 - Ports, PumpController, native test env (done)
- `src/core/ports`: IClock, ISwitch, IFloatSwitch, IStore, ILog
- `src/core/PumpController`: safety state machine, state restore after reboot
- `[env:native]` with GoogleTest; fakes in `test/support/Fakes.h`

### Step 2 - LevelFilter, Settings, rules, Tank (done)
- `LevelFilter`: sliding-window median (window 1..15)
- `Settings`: one field table drives validation, persistence (`set.<name>` keys) and later the protocol;
  cross-field rule `critical < min < max <= height`; atomic batch updates
- `rules/`: `IRule`, `LevelRule` (start <= min, stop >= max), `NightRule` (veto start at night above critical)
- `Tank`: combines filter + rules + pump; rules can veto auto-start (`PumpController::AutoStart::Inhibit`)
- Pump: added minimum run time, so a stop request right after a start is refused (`TooSoon`);
  safety stops (high level, max on-time, faults) still act immediately
- `IClock` gained `LocalHour()` (for night rule)
- `.gitattributes` added (LF in repo)
- ESP32 env now builds with `-std=gnu++17` (was the Arduino default gnu++11)

### Step 3 - Versioned JSON codec (done)
- Version lives in the endpoint (`/ws/v1`); messages carry no `v`
- `docs/protocol.md`: commands (`state.get`, `pump.set`, `settings.get`, `settings.set`), events
  (`pump.state`, `tank.level`, `settings.changed`), error codes, settings table
- `core/Messages`: transport-independent `Command`, `ErrorCode`, result-to-error mapping
- `adapters/codec/CodecV1`: `DecodeCommand` (strict validation, 1024-byte limit) and encoders
- Float switches are the primary pump control: `Tank` ignores a rule's WantOn unless `SetRulesMayStart(true)`
- Error codes: added `UNKNOWN_FIELD`, `PUMP_FAULT`; removed `SENSOR_FAULT` (constitution updated)
- Native env now also builds `adapters/codec` and uses ArduinoJson

### Step 4 - Adapters, CommandHandler, NetworkWatchdog (done)
- core: `CommandHandler` (executes decoded commands; settings are saved or rolled back),
  `NetworkWatchdog` (check -> reconnect -> restart escalation), ports `IDistanceSensor`, `INetwork`, `IRestarter`
- settings: added `watchdogDelayS` (60) and `maxReconnects` (10)
- adapters (compile-checked only, NOT yet run on hardware): `RelaySwitch`, `XkcFloatSwitch`,
  `A02yyuwDistanceSensor`, `SpiffsStore` (writes only changed values, temp-file + rename),
  `SntpClock` (non-blocking), `WifiNetwork` (non-blocking, pings the gateway), `EspLog`, `EspRestarter`,
  `WebSocketServer` (/ws/v1; decodes in the network task, queues commands for the control task)
- Not wired into `main.cpp` yet (old firmware still runs) - that is step 5

### Steps 5 + 6 - Composition root, profile, new firmware, old code removed (done)
- Settings are now signed 32-bit; new `utcOffsetMinutes` (default -180) makes the time zone configurable
- `app/Controller` (native-tested): settings, pump, tank, rules, command handling, event publishing
- `app/Firmware`: control task (owns all logic), sensor task (queue to control), network task (watchdog)
- `app/Profile` + `profiles/EspWroverKit.h`; `main.cpp` replaced
- Old firmware removed (`services/`, `os/`, `domain/`, `driver/`, `config/`, old `App`); still in git history
- Removed unused libraries WiFiManager and NewPing (HC-SR04 / UART drivers were removed with the old code)

## Hardware checklist (nothing below has run on the board yet)
1. Check the float-switch wiring: low sensor on GPIO 26, high on GPIO 27 (old code swapped them). With the pump
   disconnected, wet/dry each sensor and read `state.get`; wrong wiring shows a `sensorInconsistent` fault.
2. Confirm the relay is off at boot and switches with `pump.set`.
3. Confirm WiFi connects, time syncs (pump leaves the `noTime` fault) and `ws://<ip>/ws/v1` answers `state.get`.
4. Watch stack headroom (debug log every 10 s) and tune the task stack sizes in `Firmware.cpp`.
5. Set `utcOffsetMinutes` for your location if it is not UTC-3.

## Open Questions / Assumptions to confirm
- Level formula: `level = height - (distance + filteredDistanceOffsetCm)`, clamped to 0..height
  (sensor on top looking down). The old code never converted distance to level.
- Night rule does nothing when the level or the clock is unknown (pump is not blocked).
- Old settings not carried over yet (added back when a feature needs them): weatherDelayS, watchdog*,
  wifiReconnectTimeoutS, maxServiceRestarts, minFreeHeapThresholdKb, maxErrorReports.
  `medianMeasurementCount` and `medianWindow` were duplicates; only `medianWindow` remains.

## Rejection Log
None.
