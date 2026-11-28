# Implementation Progress
Branch: `refactor/ports-and-adapters` · Plan: see "Migration Plan" in `constitution.md`

## Summary
- **Steps Completed:** 6 / 6 (steps 5 and 6 done together)
- **Current Step:** Done - hardware checklist completed on the board (item 4, A02YYUW_UART, skipped for now)
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
- adapters (compile-checked only, NOT yet run on hardware): `RelaySwitch`, `XKC_Y23`,
  `A02YYUW_PWM`, `A02YYUW_UART`, `SpiffsStore` (writes only changed values, temp-file + rename),
  `SntpClock` (non-blocking), `WifiNetwork` (non-blocking, pings the gateway), `EspLog`, `EspRestarter`,
  `WebSocketServer` (/ws/v1; decodes in the network task, queues commands for the control task)
- Not wired into `main.cpp` yet (old firmware still runs) - that is step 5

### Steps 5 + 6 - Composition root, profile, new firmware, old code removed (done)
- Settings are now signed 32-bit; new `utcOffsetMinutes` (default -180) makes the time zone configurable
- `app/Controller` (native-tested): settings, pump, tank, rules, command handling, event publishing
- `app/Firmware`: control task (owns all logic), sensor task (queue to control), network task (watchdog)
- `app/Profile` + `profiles/EspWroverKit.h`; `main.cpp` replaced. The profile picks the distance sensor
  (`A02YYUW_PWM` or `A02YYUW_UART`); its pins are commented as the A02YYUW RX/TX and the XKC_Y23 Low/High
- Old firmware removed (`services/`, `os/`, `domain/`, `driver/`, `config/`, old `App`); still in git history
- Removed unused libraries WiFiManager and NewPing. The HC-SR04 driver was dropped; `A02YYUW_UART` was ported
  (`A02YYUW_PWM` and `XKC_Y23` keep their original names)
- `A02YYUW_UART`: the old code computed `((high << 8) | low) * 100`, but the sensor reports millimetres, so the new
  driver uses `mm / 10` (cm). The frame parser (`A02YYUW_Frame`) is native-tested

### REST API (done)
- `docs/rest-api.md`: `GET /api/v1/state`, `GET /api/v1/settings`, `POST /api/v1/pump`, `PUT /api/v1/settings`
  (read + control, no authentication - chosen by the project owner)
- `adapters/codec/CodecCommon` holds the JSON shapes shared by WebSocket and REST; `RestCodec` has the REST bodies
  and HTTP status mapping (native-tested)
- `adapters/net/RestApi`, `CommandQueue` (+ `RestTicket`), `SnapshotCache`; WebSocket and REST share one
  `AsyncWebServer` and one command queue. Reads are served from a cache; writes wait up to 2 s for the control task
- `Controller::Execute()` and `Revision()`; the control task sleeps on the command queue, so commands run at once
- Compile-checked only: the REST endpoints have NOT been exercised on the board yet (see the REST checks below)
- Profile field `webSocketPort` renamed to `httpPort`

### Boot crash after adding REST: duplicate AsyncTCP (fixed)
- Symptom: boot loop with `LoadProhibited` (address 0x138) right after WiFi start; before that every REST path
  answered 500. The backtrace (decoded with addr2line against the built ELF) pointed at `AsyncWebServer::onNotFound`.
- Cause: `platformio.ini` listed `ottowinter/AsyncTCP-esphome` (1.2.1) while the web server library depends on
  `esphome/AsyncTCP-esphome` (2.1.4). Our sources were compiled against 1.2.1 and the server library against 2.1.4;
  `AsyncServer` has a different size in the two, so our `AsyncWebServer` member was too small and the library's
  constructor wrote past it. This duplicate was in the original project too, so earlier builds were exposed as well.
- Fix: removed the explicit AsyncTCP entry from `lib_deps`; only one copy remains. After any change to the libraries,
  check that `.pio/libdeps/<env>` has a single `AsyncTCP-esphome`.
- Also: unknown paths now answer 404 (the library answers 500 unless a not-found handler is set).

### Sensor logging (done)
- `XKC_Y23` (tag `XKC_Y23`, Info): logs the initial state of the Low and High sensors, then every change
  ("Low: water detected" / "Low: no water", same for High)
- `A02YYUW` (tag `A02YYUW`, Info): logs every new median once the window is full, with the water level:
  "New median: 90.0 cm (water level 50.0 cm)". The median is sliding, so this is one line per reading
  (every `radarDelayS`). Failed readings are not logged here.
- Done in `app/Controller`, so it works with either A02YYUW mode and is covered by native tests

## Hardware checklist (verified on the board by the project owner)
0. [done] A02YYUW pins follow your wiring, which is the reverse of the old Pins.hpp: GPIO 19 = ESP RX <- A02YYUW TX,
   GPIO 18 = ESP TX -> A02YYUW RX.
1. [done] Check the XKC_Y23 wiring: Low on GPIO 26, High on GPIO 27 (old code swapped them). With the pump
   disconnected, wet/dry each sensor and read `state.get`; wrong wiring shows a `sensorInconsistent` fault.
2. [confirmed] Confirm the relay is off at boot and switches with `pump.set`.
3. [confirmed] Confirm WiFi connects, time syncs (pump leaves the `noTime` fault) and `ws://<ip>/ws/v1` answers `state.get`.
4. [skipped for now] If you use `A02YYUW_UART`, set `distanceSensor` in the profile and check the RX/TX wiring (a swapped pair
   gives no readings, so the level stays unknown).
5. [done] Watch stack headroom (debug log every 10 s) and tune the task stack sizes in `Firmware.cpp`.
6. [done] Set `utcOffsetMinutes` for your location if it is not UTC-3.

Consequences: the early start of the WebSocket server (before WiFi is connected) works, and the
A02YYUW_PWM wiring and the XKC_Y23 pins are correct as in `EspWroverKit.h`. `A02YYUW_UART` is still untested on hardware.

## REST checks on the board (not done yet)
1. `curl http://<ip>/api/v1/state` and `/api/v1/settings` return JSON.
2. `POST /api/v1/pump` with `{"on":true}` switches the pump (pump disconnected first); a second call right away
   answers `409 PUMP_TOO_SOON`.
3. `PUT /api/v1/settings` with a valid and an invalid value (`200`, then `422`).
4. A write without `Content-Type: application/json` answers `415`.
5. The WebSocket at `/ws/v1` still works while REST is used, and sees the events caused by REST writes.

## Open Questions / Assumptions to confirm
- Level formula: `level = height - (distance + filteredDistanceOffsetCm)`, clamped to 0..height
  (sensor on top looking down). The old code never converted distance to level.
- Night rule does nothing when the level or the clock is unknown (pump is not blocked).
- Old settings not carried over yet (added back when a feature needs them): weatherDelayS, watchdog*,
  wifiReconnectTimeoutS, maxServiceRestarts, minFreeHeapThresholdKb, maxErrorReports.
  `medianMeasurementCount` and `medianWindow` were duplicates; only `medianWindow` remains.

## Rejection Log
None.
