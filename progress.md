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

### OPEN: heap corruption crash after the WebSocket check (not fixed yet)
- Symptom: `CORRUPT HEAP: Bad head at 0x3ffda5d4 ... assert failed: multi_heap_free`, then reboot. Decoded backtrace:
  `tcp_close` in lwIP (called from `_tcp_close_api` of AsyncTCP) freeing a TCP connection, i.e. when a connection closed.
  The firmware on the board matched the build (same ELF checksum). Seen after `tools/ws-check.ps1` had run.
- That is only where it was noticed: something wrote outside its memory earlier.
- Main suspect (not proven): WebSocket events/answers are sent from the control task (`AsyncWebSocket::text/textAll`),
  but the library deletes a client in the network task when it disconnects, and its client list has no lock
  (neither the ottowinter/esphome copy 3.4.0 nor mathieucarbou's 3.6.0 locks it). A client closing during a send is a
  use-after-free. The destructor of the client frees its queues before it announces the disconnect, so a registry of
  my own cannot close that window.
- Diagnostic added: the control task checks the whole heap every 2 s and logs `HEAP CORRUPTION DETECTED` right after the
  damage happens, next to the `REST`/`WS` lines that show what was going on (`kHeapCheckEveryLoops` in `Firmware.cpp`).
- Evidence so far: two crashes (old build, then the build with the heap check), both about 30 s after the WebSocket
  client connected, i.e. when `ws-check` closed the connection, with the same backtrace (`tcp_close`). The second one
  was a "Bad tail" instead of "Bad head", and the 2-second heap check logged nothing before it, so the damage is done
  during the disconnect itself. REST (all work in the network task) never crashed over dozens of calls.
- Experiment firmware: `pio run -e esp-wrover-kit-nowssend -t upload` is identical but the WebSocket never sends
  anything (`WS_DISABLE_SENDS`). Run `tools\ws-check.ps1` three times: no crash = sending from the control task is
  the cause; still crashes = the problem is in the library/connection handling itself.
- First experiment attempt was inconclusive: the script received answers and events, so the board ran the NORMAL
  firmware (in the no-send build `ws-check` prints "Answers to commands: 0"). On the normal firmware three disconnects
  in a row did not crash. So the crash is intermittent (consistent with a race, but three clean runs prove nothing).
- Next: `tools\ws-stress.ps1` connects and disconnects many times at random moments (optionally dropping the
  connection abruptly) and then checks whether the board still answers. Run it against both builds:
  `.\tools\ws-stress.ps1 -Cycles 60 -Abrupt` on the normal firmware (expected: a crash within a few dozen cycles if
  the race theory is right) and on `esp-wrover-kit-nowssend` (expected: no crash). Compare.
- RESULT of the stress runs (`ws-stress.ps1 -Cycles 60 -Abrupt`):
  - normal firmware (ELF 5d899617f5910ac3): crashed on cycle 15 (CORRUPT HEAP, `tcp_close`).
  - no-send firmware (ELF a3ab9cbbeafaa9d3, confirmed by `ws-check`: 0 answers, no events): crashed too, twice (cycle 17,
    and about 13 s after reboot with only 4 clients). Same backtrace. The corrupted header holds 0x3ffc8d04 every time
    (a heap address at the very start of the heap, same in both builds).
  - So the theory "sending from the control task races with the disconnect" is WRONG. The crash is in the connection
    handling itself: `tcp_close` called by AsyncTCP (`_tcp_close_api`), on lwIP's TCP thread.
- Likely cause now: the combination AsyncTCP-esphome 2.1.4 (what ESPAsyncWebServer-esphome 3.4.0 depends on) with the
  Arduino core 2.0.17 / IDF 4.4 that the official platform ships, or a bug in how AsyncTCP closes connections.
  Not proven. REST has not been stress tested; ~30 manual calls were fine.
- The Arduino core has the built-in HTTP server with WebSocket support (`CONFIG_HTTPD_WS_SUPPORT 1`,
  `httpd_ws_send_frame_async` is meant to be used from other tasks); it is the alternative to the async libraries.
- Options if the WebSocket is confirmed as the cause: (a) a patched copy of the web server library in `lib/` with a
  lock around the client list, (b) send WebSocket data only from the network task, (c) keep REST only. REST is not
  affected: its handlers run in the network task.

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

### PUT /api/v1/settings answered INTERNAL (fixed, to confirm on the board)
- Symptom: `PUT {"minLevelCm":60}` -> `500 {"ok":false,"err":"INTERNAL"}`; the pump `POST` worked.
- Likely cause: `SpiffsStore` rewrote its file (write temp file, delete, rename) once per key, so the first save of the
  16 settings was 16 slow SPIFFS rewrites. That can exceed the 2 s REST wait, or a write failed. Nothing logged which.
- Fix: `IStore` now has `Commit()`: `SetI64` only stages values and `Commit()` writes the file once (and only if
  something changed). `SaveSettings` and the pump state commit once. A failed save is now logged
  ("Command failed: the settings could not be saved", tag `Controller`).
- If it still answers INTERNAL after reflashing, the serial log shows the SPIFFS error (CORE_DEBUG_LEVEL is 5).

### REST call logging (done)
- Tag `REST`, Info: "<METHOD> <path> from <client ip>" (writes add the body) when a call arrives, and
  "<METHOD> <path> -> <status>" when it is answered (writes and errors add the answer body).
- WebSocket (tag `WS`, Info): clients connecting/leaving, each command received and each answer sent (cut after
  100 characters). Pushed events are not logged (the level event fires about once a second).
- `PUT /api/v1/settings` was confirmed on the board after the batched-store fix (both `PUT`s answered `ok`).

### Sensor logging (done)
- `XKC_Y23` (tag `XKC_Y23`, Info): logs the initial state of the Low and High sensors, then every change
  ("Low: water detected" / "Low: no water", same for High)
- `A02YYUW` (tag `A02YYUW`, Info): once the window is full it logs at most every 10 seconds (the first one right
  away), with the water level and the last 15 distance medians (oldest first, whole numbers, `cm` only after the last):
  "New median: 90.0 cm (water level 50.0 cm) (88, 89, 90 cm)". Fewer than 15 values are shown until 15 exist.
  Failed readings are not logged here. The 10 s interval is `kMedianLogIntervalS` in `app/Controller.h`.
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

## REST checks on the board
1. [confirmed] `GET /api/v1/state` and `/api/v1/settings` return JSON; unknown paths answer 404.
2. [confirmed] `POST /api/v1/pump` switches the pump; refusals seen: `HIGH_LEVEL` (high XKC_Y23 touched by hand)
   and `PUMP_TOO_SOON`.
3. [confirmed] `PUT /api/v1/settings`: valid value `ok`; invalid values `INVALID_VALUE`
   (`minLevelCm` 0 is below `criticalLevelCm`).
4. [confirmed] Wrong content type (`text/plain`) answers 415.
5. [open] The WebSocket at `/ws/v1` still works while REST is used, sees the events caused by REST writes, and the
   `WS` log lines appear. Run `.\tools\ws-check.ps1` (tested against a fake local server, not yet against the board).

## Open Questions / Assumptions to confirm
- Level formula: `level = height - (distance + filteredDistanceOffsetCm)`, clamped to 0..height
  (sensor on top looking down). The old code never converted distance to level.
- Night rule does nothing when the level or the clock is unknown (pump is not blocked).
- Old settings not carried over yet (added back when a feature needs them): weatherDelayS, watchdog*,
  wifiReconnectTimeoutS, maxServiceRestarts, minFreeHeapThresholdKb, maxErrorReports.
  `medianMeasurementCount` and `medianWindow` were duplicates; only `medianWindow` remains.

## Rejection Log
None.
