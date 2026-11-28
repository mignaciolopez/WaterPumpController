# WaterPumpController

Firmware for an **ESP32** that keeps a water tank filled. It watches the water level with two level sensors
and an ultrasonic distance sensor, switches a pump through a relay, and can be read and controlled over
**WiFi** (REST API and WebSocket).

> **Safety.** This controls a pump, possibly on mains voltage. Have the mains side wired by a qualified person,
> and do not rely on this firmware as your only protection against overflow or a dry-running pump. See
> [Safety and security](#safety-and-security).

## Contents

- [What it does](#what-it-does)
- [How the pump is controlled](#how-the-pump-is-controlled)
- [Hardware](#hardware)
  - [Tested sensors](#tested-sensors)
  - [Wiring](#wiring)
- [Getting started](#getting-started)
- [Using it](#using-it)
  - [REST API](#rest-api)
  - [WebSocket](#websocket)
  - [Settings](#settings)
  - [What the serial console shows](#what-the-serial-console-shows)
- [Troubleshooting](#troubleshooting)
- [Safety and security](#safety-and-security)
- [Development](#development)
- [Limitations](#limitations)
- [More documentation](#more-documentation)

## What it does

- Starts the pump when the **low** level sensor stops seeing water, and stops it when the **high** sensor sees
  water.
- Protects the pump: minimum run and rest times, a maximum run time followed by a cooldown, and a safe stop when
  the sensors disagree or the clock is not available.
- Measures the water level with an ultrasonic sensor (median-filtered) and reports it. The distance sensor does
  **not** control the pump.
- Serves a **REST API** and a **WebSocket** (live pump state and level) from the board itself.
- Stores its settings and the pump state in flash, so cooldown survives a reboot.
- Watches the network: it pings the router regularly, reconnects WiFi when needed and restarts the board as a
  last resort. The pump logic does not depend on the network.

## How the pump is controlled

The pump acts **only on the two XKC_Y23 level sensors** (plus its own safety limits below). The distance sensor only
reports the level, in the log and through the API; it never starts or stops the pump.

| Situation | What happens |
|---|---|
| Low sensor reports **no water** | The pump starts (if allowed, see below) |
| High sensor reports **water** | The pump stops at once |
| Pump has run `pumpMaxTimeOnM` minutes (default 20) | Forced stop, then **cooldown** for `pumpCooldownTimeM` minutes (default 60): no start until it ends |
| Stop requested within `pumpMinRunS` (10 s) of starting | Refused (`PUMP_TOO_SOON`) so the pump does not cycle. Safety stops are never delayed |
| Start requested within `pumpMinRestartS` (10 s) of stopping | Refused (`PUMP_TOO_SOON`) |
| High sensor wet while the low sensor is dry | **Fault `sensorInconsistent`**: pump off. Usually the two sensors are swapped or one is broken. Clears by itself when the readings make sense |
| Clock not synced yet (first ~30 s after boot) | **Fault `noTime`**: pump off. Clears by itself when the time arrives |

Other rules worth knowing:

- The relay is switched **off first thing at boot**. After a reboot the pump counts as stopped, so the restart delay
  and any cooldown still apply.
- The distance sensor **cannot start or stop the pump**. Only the XKC_Y23 sensors, the safety limits, or a manual command do.
  The night mode and the early stop at `maxLevelCm` that used the distance sensor are switched off; the settings
  `criticalLevelCm`, `minLevelCm`, `maxLevelCm`, `nightStartHour` and `nightEndHour` are still accepted and reported but
  **currently unused**. (`heightCm` and `filteredDistanceOffsetCm` still define the reported level.)
- If the distance sensor fails, the level just becomes unknown (`null`). The pump keeps working from the XKC_Y23
  sensors.
- **Blind zone:** the A02YYUW cannot measure closer than 3 cm (or farther than 450 cm): it just gives no echo. So the
  firmware watches the readings **before** that happens. A reading within 2 cm of the limit (5 cm or less) is kept as
  a normal measurement, and if the sensor then goes silent, the water is taken to be inside the blind zone: the
  distance is reported as 3 cm (a level of `heightCm - 3`, full) and stays so while the silence lasts. This is only
  believed while the **low** XKC_Y23 sensor sees water; if that one is dry, silence is a failure and the level is
  `null`, so a dead sensor is never mistaken for a full tank. If the board restarts while the water is already in
  the blind zone there is no earlier reading, so silence while the **high** XKC_Y23 sensor sees water is also read
  as "blind zone". Silence after an ordinary reading (say 90 cm) is always a failure.

## Hardware

- **Board:** ESP32 WROVER kit (PlatformIO board `esp-wrover-kit`). Other ESP32 boards should work with a new profile
  (see [Development](#development)); only the WROVER kit has been tested.
- **Relay module** driving the pump (the firmware drives it **active HIGH**: GPIO high = pump on).
- **Two XKC_Y23 contactless level sensors** (low and high).
- **One A02YYUW ultrasonic distance sensor.**
- Optional: **a DHT11 (or DHT22)** temperature/humidity sensor. A profile without one simply has no weather.
- A 2.4 GHz WiFi network and internet access for the time (NTP).

### Tested sensors

| Sensor | Role | Status |
|---|---|---|
| XKC_Y23 (contactless liquid level) x2 | Low and high level | **Tested on the board** |
| A02YYUW ultrasonic, pulse (trigger/echo) mode | Water level by distance | **Tested on the board** (default in the profile) |
| A02YYUW ultrasonic, UART mode | Same, over a serial port | Implemented; the frame parser is unit tested, but **not tested on hardware** |
| DHT11 temperature/humidity (DHT22 also supported in the profile) | Air temperature and humidity at the tank | Implemented and unit tested (readings, validation, publishing); **not yet tested on this board**. The same driver is ported from the earlier `WaterTank` project |

### Wiring

Pins come from `src/app/profiles/EspWroverKit.h`.

| Function | ESP32 GPIO | Connects to | Notes |
|---|---|---|---|
| Pump relay | **25** | Relay module input | Active HIGH. Off at boot |
| XKC_Y23 **Low** | **26** | Sensor output | Input with pull-up; the firmware reads **LOW = water detected** |
| XKC_Y23 **High** | **27** | Sensor output | Same as above |
| A02YYUW **TX** | **19** | ESP32 RX | The sensor's TX goes to GPIO 19 (echo) |
| A02YYUW **RX** | **18** | ESP32 TX | The sensor's RX goes to GPIO 18 (trigger) |
| DHT11 **data** | **21** | Sensor data pin | Optional. The module needs a pull-up on the data line (most DHT breakout boards have one; a bare sensor needs about 4.7-10 kOhm to 3.3 V) |

```
                  +-------------------+
   Relay IN  <----+ GPIO25            |
   XKC Low   ---->+ GPIO26            |
   XKC High  ---->+ GPIO27   ESP32    |
   A02YYUW TX --->+ GPIO19  (WROVER)  |
   A02YYUW RX <---+ GPIO18            |
   DHT11 data <-->+ GPIO21            |
   3V3 / 5V / GND +-------------------+   (each module on the supply its datasheet asks for;
                                           all grounds joined)
```

Placement:

- **Low sensor:** at the water level where the pump should **start**. **High sensor:** where it should **stop**.
  The low sensor must always be at or below the high one. If they are swapped the pump reports a
  `sensorInconsistent` fault and stays off.
- **Distance sensor:** on top of the tank, looking down at the water. The firmware computes
  `level = tank height - (distance + offset)`.
- **Signal levels:** ESP32 GPIOs are **3.3 V** and are not 5 V tolerant. Check what voltage each module powers on and
  outputs, and use a level shifter or divider where needed.
- **XKC_Y23 polarity:** variants of this sensor differ. This firmware expects the output to go **LOW when it detects
  water**. If yours is the opposite, the change belongs in `src/adapters/sensors/XKC_Y23.cpp`.
- **Relay:** the module must switch on when the GPIO is **HIGH**. If yours is active LOW, change
  `src/adapters/actuators/RelaySwitch.cpp`.

To use the **UART** version of the distance sensor, set `distanceSensor` to `DistanceSensorType::A02YYUW_UART` in
the profile, and wire the sensor's TX to the profile's `distanceRxPin` (and RX to `distanceTxPin`). `distanceSerial`
(1 or 2) picks the serial port.

## Getting started

### 1. Requirements

- [PlatformIO](https://platformio.org/) (CLI or the VS Code / CLion plugin).
- A USB cable to the board.
- Optional, for the unit tests on your PC: a C++ compiler on the PATH. On Windows,
  `winget install BrechtSanders.WinLibs.POSIX.UCRT` works.

### 2. WiFi credentials

Copy the template and fill it in. The real file is git-ignored.

```
copy include\secrets.h.template include\secrets.h      (Windows)
cp include/secrets.h.template include/secrets.h        (Linux / macOS)
```

```cpp
#define WIFI_SSID "your_wifi_ssid_here"
#define WIFI_PASS "your_wifi_password_here"
```

### 3. Build and upload

```
pio run -e esp-wrover-kit -t upload
pio device monitor            # 115200 baud, to watch the log
```

### 4. First boot

- The relay is switched off, the sensors are read and WiFi connects (a few seconds; an `AUTH_EXPIRE` line during the
  first reconnects is normal).
- The pump stays in a `noTime` fault for about 30 seconds until the clock is synchronized. Then you see
  `Fault cleared`, and the pump may start if the low sensor is dry.
- The board's IP address appears in the log (`STA IP: 192.168.x.y`). Everything below uses it.
- Set your time zone with `utcOffsetMinutes` (default -180, UTC-3) so that the clock and the logs match your day.

## Using it

Replace `192.168.100.247` with your board's address. Both interfaces use port **80**.

### REST API

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api/v1/state` | Pump state and water level |
| `GET` | `/api/v1/settings` | All settings |
| `POST` | `/api/v1/pump` | Body `{"on":true}` switches the pump (safety rules still apply) |
| `PUT` | `/api/v1/settings` | Body `{"minLevelCm":60,...}` changes settings (all or nothing) |

Windows PowerShell (note: plain `curl` is an alias for `Invoke-WebRequest` there):

```powershell
Invoke-RestMethod http://192.168.100.247/api/v1/state
Invoke-RestMethod -Method Post -Uri http://192.168.100.247/api/v1/pump -ContentType 'application/json' -Body '{"on":true}'
Invoke-RestMethod -Method Put  -Uri http://192.168.100.247/api/v1/settings -ContentType 'application/json' -Body '{"pumpMaxTimeOnM":15}'
```

Linux / macOS / Git Bash:

```
curl http://192.168.100.247/api/v1/state
curl -X POST http://192.168.100.247/api/v1/pump -H "Content-Type: application/json" -d '{"on":true}'
```

`GET /api/v1/state` looks like this (`level.cm` is `null` while the level is unknown; `sensor` is the range the
distance sensor can measure, in cm of distance, so a client can scale its display; `weather` is only present when a
DHT sensor is configured and its values are `null` until the first valid reading):

```json
{"pump":{"mode":"running","on":true,"cooldown":false,"fault":"none","onAt":1795875075,"offAt":1795874874,"cooldownAt":0},"level":{"cm":72.2},"sensor":{"minDistanceCm":3,"maxDistanceCm":450},"weather":{"temperatureC":23.4,"humidity":61.0,"heatIndexC":24.0}}
```

A refused request gets an HTTP error and a code, for example `409` `{"ok":false,"err":"PUMP_COOLDOWN"}`. Writes
must send `Content-Type: application/json`. Full details and the error table: [docs/rest-api.md](docs/rest-api.md).

### WebSocket

Connect to `ws://192.168.100.247/ws/v1`. The version is part of the address, so messages carry no version field.

```json
{"cmd":"state.get","id":1}                         ->  {"id":1,"ok":true,"data":{...}}
{"cmd":"pump.set","id":2,"data":{"on":false}}      ->  {"id":2,"ok":true}
{"cmd":"settings.set","id":3,"data":{"minLevelCm":60}}
```

The device also **pushes events**: `pump.state` when the pump changes, `tank.level` when the level changes,
`tank.weather` when the temperature or humidity changes (DHT sensor only), `settings.changed` after a settings change. A client gets nothing on connect, so send `state.get` first.
Full protocol: [docs/protocol.md](docs/protocol.md).

Quick test from Windows PowerShell, no extra tools needed:

```powershell
.\tools\ws-check.ps1 -Ip 192.168.100.247 -Seconds 30
```

It sends `state.get` and `settings.get` and prints everything the board sends. Make a REST call from another window
to see its events arrive.

### Settings

Change any of these with `PUT /api/v1/settings` (or `settings.set` over the WebSocket). They are saved in flash.
Rule: `criticalLevelCm < minLevelCm < maxLevelCm <= heightCm`.

| Name | Default | Range | Meaning |
|---|---|---|---|
| `heightCm` | 140 | 20..450 | Tank height |
| `criticalLevelCm` | 20 | 0..450 | *Currently unused* (night mode is off) |
| `minLevelCm` | 50 | 0..450 | *Currently unused* |
| `maxLevelCm` | 130 | 0..450 | *Currently unused* (the distance sensor no longer stops the pump) |
| `filteredDistanceOffsetCm` | 0 | 0..100 | Distance sensor offset |
| `radarDelayS` | 1 | 1..3600 | Seconds between distance readings |
| `medianWindow` | 5 | 1..15 | Readings used for the median |
| `weatherDelayS` | 30 | 2..3600 | Seconds between temperature/humidity readings |
| `pumpMinRunS` | 10 | 0..600 | Minimum seconds ON before a stop request is accepted |
| `pumpMinRestartS` | 10 | 0..600 | Minimum seconds OFF before the next start |
| `pumpMaxTimeOnM` | 20 | 1..240 | Longest continuous run (minutes), then forced stop + cooldown |
| `pumpCooldownTimeM` | 60 | 1..1440 | Lockout after a max-on-time stop (minutes) |
| `nightStartHour` | 22 | 0..23 | *Currently unused* (night mode is off) |
| `nightEndHour` | 8 | 0..23 | *Currently unused* (night mode is off) |
| `utcOffsetMinutes` | -180 | -720..840 | Local time = UTC + this (minutes, no daylight saving) |
| `watchdogDelayS` | 60 | 10..3600 | Seconds between network checks |
| `maxReconnects` | 10 | 1..100 | Failed reconnect rounds before the board restarts |

### What the serial console shows

At 115200 baud. Tags tell you where a line comes from:

```
[XKC_Y23] Low: no water                       sensor state at start, then on every change
[A02YYUW] New median: 67.4 cm (water level 72.6 cm) (67, 67, 68, ... 67 cm)
                                              every 10 s: median distance, level, last 15 medians
[DHTDevice] 23.4 C, 61 % humidity (heat index 24.0 C)   every weatherDelayS (30 s)
[PumpController] Pump ON / Pump OFF           every pump change
[PumpController] Fault: clock not synced      pump held off; clears by itself
[PumpController] Pump on cooldown: 37 min left   while on cooldown: when it starts, then every 10 s
[REST] POST /api/v1/pump from 192.168.100.3 body: {"on":true}
[REST] POST /api/v1/pump -> 200 {"ok":true}   every REST call, when it arrives and how it was answered
[WS] Client #54 connected from 192.168.100.3  WebSocket clients, commands and answers
[NetworkWatchdog] Network check failed: ...   says whether the link is down or the ping failed
```

The log level is verbose (`CORE_DEBUG_LEVEL=5` in `platformio.ini`), so WiFi internals also appear. Lower it to
reduce the noise.

## Troubleshooting

| Symptom | Likely cause and what to do |
|---|---|
| Pump never starts, `fault: "noTime"` | The clock has not synced. Wait about 30 s; check the board has internet access to `pool.ntp.org` |
| `fault: "sensorInconsistent"` | High sensor is wet while the low one is dry: the two are swapped or one is faulty. Check wiring (low = GPIO 26, high = GPIO 27) and sensor heights |
| `POST /api/v1/pump` answers `HIGH_LEVEL` | The high sensor reports water: the tank is full |
| ... answers `PUMP_COOLDOWN` | The pump ran too long; wait for the cooldown (`pumpCooldownTimeM`). The log shows `Pump on cooldown: N min left` every 10 s |
| ... answers `PUMP_TOO_SOON` | Stop/start requested too soon after the last change; wait `pumpMinRunS` / `pumpMinRestartS` seconds |
| Level shows `null` | Not enough valid distance readings yet (needs `medianWindow` readings), or the sensor is not wired / out of range (3-450 cm), or the sensor goes silent after an ordinary reading. When the water rises into the blind zone (closer than 3 cm), the level should read `heightCm - 3` instead (see the blind zone note above); the log then says `No echo right after a reading near the minimum range` |
| `weather` values are `null` | The DHT gave no valid reading yet (it needs about 2 s after power-up) or three readings in a row failed. Check the data wire and pull-up on GPIO 21; the log shows `Weather read invalid` |
| Can't reach the board | Look for the IP in the log. Same network? `ping <ip>`. In PowerShell use `Invoke-RestMethod`, not `curl` |
| `Network check failed: the WiFi link is down` | WiFi dropped. The board reconnects by itself and restarts after `maxReconnects` failed rounds |
| `Command failed: the settings could not be saved` | Flash storage problem; the change was rolled back |
| `HEAP CORRUPTION DETECTED` | Should not happen any more; if it does, keep the log and the lines before it |

## Safety and security

- **Not a certified safety device.** The firmware stops the pump when the high sensor reports water, and limits run
  time, but a stuck sensor, a stuck relay or a software fault can still leave it running. Use an **independent
  overflow or dry-run protection** (for example a mechanical float valve or a separate cutoff) where a failure would
  cause damage.
- **Fail-safe defaults:** the relay is off at boot; faults turn the pump off; the pump logic does not need the
  network.
- **Mains wiring:** have it done by a qualified person.
- **No authentication.** Anyone who can reach the board on your network can read its state and switch the pump,
  over REST and over the WebSocket. Keep it on a trusted network and **never expose port 80 to the internet**.
  Write requests must send `Content-Type: application/json` and the board sends no CORS headers, which stops a web
  page on another site from using REST, but does not stop tools or other devices on your network.
- **WiFi password** lives in `include/secrets.h` (git-ignored). Do not commit it.

## Development

### Project layout

The code follows ports and adapters: the rules live in plain C++ that does not know about the hardware, and the
hardware sits behind small interfaces.

```
src/
  core/        Pump rules, filter, settings, tank, rules, command handling, network watchdog.
               No Arduino or ESP code; tested on the PC.
    ports/       Interfaces: clock, switch, sensors, store, log, network, message sink
  adapters/    Implementations of those interfaces
    sensors/     XKC_Y23, A02YYUW_PWM, A02YYUW_UART (+ frame parser)
    actuators/   RelaySwitch
    storage/     SpiffsStore (settings and pump state in flash)
    platform/    WiFi, SNTP clock, logging, restart
    net/         HttpServer, RestApi, WebSocketServer
    codec/       JSON formats of the WebSocket (v1) and REST
  app/         Controller (application logic, tested), Firmware (tasks), profiles/ (pins per board)
test/          Unit tests (GoogleTest, run on the PC)
tools/         PowerShell scripts to try and stress the board
docs/          protocol.md (WebSocket), rest-api.md (REST)
```

At run time three tasks work: a **control task** that owns all pump logic, a **sensor task** that measures the
distance, and a **network task** that runs the watchdog. The HTTP server runs in its own task and hands commands to
the control task through a queue.

### Tests

```
pio test -e native                       # unit tests on your PC (needs a C++ compiler)
.\tools\ws-stress.ps1 -Cycles 100 -Abrupt   # connect/disconnect the WebSocket many times
.\tools\rest-stress.ps1 -Requests 500       # many REST requests
```

The unit tests cover all the pump rules, settings, the median filter, the watchdog, the command handling, both JSON
formats and the sensor frame parser. The stress scripts need a board and report whether it still answers afterwards.
Run both at once for the toughest check.

### Adapting it

- **Another board or other pins:** add a header like `src/app/profiles/EspWroverKit.h` and use it in `main.cpp`.
- **Another sensor or actuator:** implement the matching interface in `src/core/ports/` as a new adapter. The core does
  not change.
- **New rule:** add a class implementing `IRule` (see `src/core/rules/Rules.h`) and register it in
  `src/app/Controller.cpp`.
- **More interfaces (MQTT, ...):** write a new adapter; commands and events already exist in `core`.

## Limitations

- One tank and one pump per board.
- Local time uses a fixed offset (`utcOffsetMinutes`): **no daylight saving**.
- At most 6 simultaneous connections (REST and WebSocket together); the one idle the longest is dropped when more
  arrive. REST bodies are limited to 512 bytes, WebSocket messages to 1024 bytes.
- A WebSocket client receives nothing when it connects; it has to ask with `state.get`.
- Only the WROVER kit, the XKC_Y23 level sensors and the A02YYUW in pulse mode have been tested on hardware.
- Settings saved by older firmware versions are not migrated.
- No license file has been added yet.

## More documentation

- [docs/rest-api.md](docs/rest-api.md): REST endpoints, errors, PowerShell examples.
- [docs/protocol.md](docs/protocol.md): WebSocket protocol, events, settings, error codes.
- [constitution.md](constitution.md): architecture rules and conventions for working on this code.
- [progress.md](progress.md): how the project was built, what was verified on the board, and the bugs found on the way
  (including the memory crash and the leaked sockets).
