# Project Constitution: WaterPumpController

Firmware for an ESP32 that monitors water-tank level and controls a pump.
This document is the source of truth for architecture and rules. Changes to it need explicit approval.

## Tech Stack
- **Target:** ESP32 (`esp-wrover-kit` first; any ESP32 with FreeRTOS via a device profile)
- **Build:** PlatformIO with the **pioarduino** platform (Arduino core 3.x, ESP-IDF 5.x, GCC 14), Arduino framework.
  Language standard: **C++23** (`-std=gnu++23`), the latest the toolchain supports. The official `espressif32@6.x` platform is capped at C++17 and is not used.
  If a required library cannot build on core 3.x, ask before falling back to a lower standard.
- **Test:** PlatformIO `native` env + GoogleTest (runs on the PC, no hardware). Unity is not used. Native env also builds with `-std=gnu++23`.
- **Libraries:** ArduinoJson, ESPAsyncWebServer-esphome, AsyncTCP-esphome, ESP32Ping (watchdog rule).
  DHT / Adafruit Unified Sensor stay for the future weather adapter only.
  Removed: WiFiManager, NewPing (unless the HC-SR04 driver is kept).
- **Storage:** SPIFFS, behind the `IStore` port
- **Time:** SNTP (ESP-IDF), behind the `IClock` port
- **Supported pin profile:** ESP32 only. The ESP32-C3 profile is dropped.

## Architecture Principles
Ports and adapters (hexagonal) with an event bus.

```
src/
  core/            Pure C++23. NO Arduino, ESP-IDF, FreeRTOS or ArduinoJson includes.
    ports/           ILevelSensor, IFloatSwitch, ISwitch, IClock, IStore, IEventBus, ILog
    PumpController   state machine: Idle -> Running -> Cooldown -> Fault
    LevelFilter      median filter
    Tank             one sensor + one pump + rules; repeatable unit
    rules/           Rule interface + rules (level, max on-time, night mode, network watchdog)
    Settings         field table (name, ptr, min, max, default); drives validation and (de)serialization
  adapters/        Implement the ports. May use Arduino/ESP-IDF.
    sensors/         A02YYUW, XKC_Y23, HC-SR04 (optional), DHT (future)
    actuators/       Relay
    storage/         SpiffsStore
    platform/        SntpClock, FreeRtosScheduler, WiFi/Network, Ping
    net/             WebSocketServer (later: MQTT, REST)
    codec/           Versioned JSON protocol (the only place that knows the wire format)
  app/             Composition root. Builds objects and wires them. No singletons.
    main.cpp
    profiles/        Per-device config: pins, tanks, enabled interfaces
test/              Native tests for core/ and codec/
docs/protocol.md   Wire protocol specification
```

Rules of the dependency direction:
- `core` depends on nothing outside `core`. `adapters` depend on `core`. `app` depends on both.
- Interfaces (WebSocket, MQTT, ...) never call services directly. They send **commands** and subscribe to **events** on the bus.
- Hardware, time, storage and logging are reached only through ports, so `core` can be tested with fakes.
- No global or singleton objects. Dependencies are passed through constructors.
- Task model: one sampling task (sensors) and one control task (rules, pump), connected by a queue.
  Shared state is owned by one task; other tasks send messages instead of sharing mutable data.
- Adding a sensor, actuator, interface, rule or tank must not require editing `core` logic
  (a new rule or port implementation is the extension point).

## Protocol (versioned JSON)
Full specification lives in `docs/protocol.md`.
- **The version is part of the endpoint, not of each message.** The WebSocket path is `/ws/v1`.
  A client connecting to an unsupported path (e.g. `/ws/v9`) is refused at connect time. Messages carry no `v`.
- A future `/ws/v2` may run alongside `/ws/v1`; both are translated by the codec to the same internal events and commands.
- Message shape is identified by its first key, with short descriptive names:
```json
{"event":"pump.state","data":{"on":true,"cooldown":false}}
{"cmd":"pump.set","id":7,"data":{"on":true}}
{"id":7,"ok":true}
{"id":7,"ok":false,"err":"PUMP_COOLDOWN"}
```
- Every command gets exactly one response with the same `id` (a small integer chosen by the client).
- Events carry no timestamp by default; state payloads include times only when they are part of the state
  (e.g. `onSince`, Unix seconds UTC). Clients may request a full snapshot with `{"cmd":"state.get","id":1}`.
- Breaking changes need a new path version; additive fields do not.
- Field names are camelCase.
- Optional later: MessagePack encoding on a `/ws/v1/bin` path if payload size ever matters.

## Boundaries
### ALWAYS DO:
- Put business and safety logic in `core` and cover it with native tests before finishing a task
- Keep the pump safety rules in `PumpController` (min switch interval, max on-time, cooldown, high-level cutoff), never in adapters or interfaces
- Fail safe: on sensor failure, lost time sync, or corrupt state, the pump goes OFF and the controller enters `Fault`
- Validate all external input (commands, settings, stored files) against the Settings field table
- Check and propagate return values of every `Setup()`/IO call (`[[nodiscard]]`)
- Document every public interface (port, event topic, command) with a short comment
- Keep the code compiling (`pio run` and `pio test -e native`) at every commit

### ASK FIRST:
- Before adding any new third-party library
- Before changing a port interface in `core/ports`
- Before changing the protocol (any change to `docs/protocol.md`)
- Before changing the stored file format
- Before changing task layout, priorities or stack sizes
- Before changing this constitution

### NEVER DO:
- Never include Arduino, ESP-IDF, FreeRTOS or ArduinoJson headers in `core/`
- Never use singletons or mutable globals
- Never hardcode secrets (WiFi credentials, tokens); use `secrets.h` (git-ignored) or provisioning
- Never block a task with long delays in the control loop (no `delay()` there)
- Never turn the pump on without checking the high-level sensor and the cooldown state
- Never add features outside the agreed spec
- Never commit generated `.pio/` content

## Code Style
- Namespace: `CE` (adapters in `CE::Adapters`, core in `CE::Core`)
- Types and methods: `PascalCase`; private members: `_camelCase`; constants: `kPascalCase`
- Interfaces start with `I` (`ISwitch`)
- Header guards: `#pragma once`; headers `.h` for classes, `.hpp` for header-only
- Max function length: 50 lines; max file length: 300 lines
- Prefer `constexpr`, `enum class`, `std::function`/interfaces over macros
- Logging through `ILog` in `core`; `ESP_LOGx` only in adapters and `app`
- No dynamic allocation in the control loop after startup

## Error Handling Standards
- `core` returns explicit results (`bool` or a small `Result`/error-code enum); exceptions are not used
- Protocol errors use the `err` field of the response with a stable code:
  `BAD_REQUEST`, `UNKNOWN_COMMAND`, `INVALID_VALUE`,
  `PUMP_COOLDOWN`, `PUMP_TOO_SOON`, `HIGH_LEVEL`, `SENSOR_FAULT`, `INTERNAL`
- Every error is logged once, at the layer that handles it
- A failed `Setup()` of a non-critical adapter (e.g. network) must not stop the pump logic from running safely

## Testing Requirements
- Native tests for all `core/` logic and the codec; target >= 80% line coverage of `core/`
- Test file naming: `test/test_<unit>/test_<unit>.cpp`
- AAA pattern: Arrange, Act, Assert
- Hardware, time, storage and network are replaced with fakes (`FakeClock`, `FakeSwitch`, `FakeStore`, `FakeSensor`)
- Required cases for `PumpController`: min switch interval, max on-time then cooldown, cooldown expiry,
  high-level cutoff, low-level start, sensor fault, state restore after reboot

## Settings Standards
- One field table describes every setting: name, type, default, min, max
- The same table drives validation, persistence and the protocol
- Invalid values are rejected with `INVALID_VALUE`; the previous value is kept

## Migration Plan
1. Add `[env:native]`, `core/ports`, fakes, and `PumpController` with tests
2. `LevelFilter`, `Settings` field table, rules, `Tank`
3. Codec + `docs/protocol.md` with tests
4. Adapters (drivers, SPIFFS, SNTP, network, WebSocket, ping watchdog rule)
5. Composition root and `profiles/esp-wrover-kit`
6. Remove old `services/`, `os/`, `domain/` code and unused libraries
Each step is a separate commit on a dedicated branch.
