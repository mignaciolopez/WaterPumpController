# WaterPumpController Protocol v1

Transport: WebSocket, text frames, one JSON object per frame.
Endpoint: `ws://<device>/ws/v1`

The version is part of the endpoint. **Messages carry no version field.**
A connection to an unsupported path (for example `/ws/v9`) is refused when it is opened.
A future `/ws/v2` can run next to `/ws/v1`.

## Message kinds

The first key tells you what a message is.

| Direction | Kind | Shape |
|---|---|---|
| client → device | command | `{"cmd":"<name>","id":<n>,"data":{...}}` |
| device → client | response | `{"id":<n>,"ok":true}` / `{"id":<n>,"ok":true,"data":{...}}` / `{"id":<n>,"ok":false,"err":"<CODE>"}` |
| device → client | event | `{"event":"<topic>","data":{...}}` |

- `id` is an integer from 0 to 2147483647 chosen by the client. Every command gets exactly one response with the same `id`.
- A command with a missing or invalid `id` gets `{"id":null,"ok":false,"err":"BAD_REQUEST"}`.
- Events are pushed to every connected client. They carry no timestamp; times inside `data` are Unix seconds (UTC).
- Field names are camelCase. Unknown extra fields in a command are ignored.
- Maximum message size: **1024 bytes**. Larger messages get `BAD_REQUEST`.

## Commands

### `state.get`
Snapshot of the pump and the water level.
```json
{"cmd":"state.get","id":1}
{"id":1,"ok":true,"data":{"pump":{...pump object...},"level":{"cm":45.2}}}
```

### `pump.set`
Turn the pump on or off. Safety rules still apply: a start can be refused (`PUMP_COOLDOWN`,
`PUMP_TOO_SOON`, `HIGH_LEVEL`, `PUMP_FAULT`) and a stop right after a start is refused
(`PUMP_TOO_SOON`). `on` must be a JSON boolean.
```json
{"cmd":"pump.set","id":7,"data":{"on":true}}
{"id":7,"ok":true}
{"id":7,"ok":false,"err":"PUMP_COOLDOWN"}
```

### `settings.get`
```json
{"cmd":"settings.get","id":2}
{"id":2,"ok":true,"data":{"heightCm":140,"criticalLevelCm":20, ...}}
```

### `settings.set`
Updates one or more settings **atomically**: either all values are applied or none. Values must be
integers (settings may be negative where the range allows). Up to 16 values per command. On success a `settings.changed` event follows.
```json
{"cmd":"settings.set","id":3,"data":{"minLevelCm":90,"maxLevelCm":100}}
{"id":3,"ok":true}
{"id":3,"ok":false,"err":"INVALID_VALUE"}
```

## Events

### `pump.state`
Sent whenever the pump state changes.
```json
{"event":"pump.state","data":{"mode":"running","on":true,"cooldown":false,"fault":"none","onAt":1767000000,"offAt":1766990000,"cooldownAt":0}}
```
- `mode`: `idle`, `running`, `cooldown`, `fault`
- `fault`: `none`, `noTime` (clock not synced), `sensorInconsistent` (high sensor wet while low sensor dry)
- `onAt`, `offAt`, `cooldownAt`: Unix seconds of the last start, last stop and cooldown start (0 = never)

### `tank.level`
Water level above the tank bottom, 0.1 cm resolution. `null` when unknown (not enough valid readings).
```json
{"event":"tank.level","data":{"cm":45.2}}
{"event":"tank.level","data":{"cm":null}}
```

### `settings.changed`
Same data object as the `settings.get` response.

## Settings

| Name | Default | Range | Meaning |
|---|---|---|---|
| `heightCm` | 140 | 20..450 | Tank height |
| `criticalLevelCm` | 20 | 0..450 | At or below this, pumping may start even at night |
| `minLevelCm` | 50 | 0..450 | Level considered low |
| `maxLevelCm` | 130 | 0..450 | At or above this, the pump is asked to stop |
| `filteredDistanceOffsetCm` | 0 | 0..100 | Distance sensor offset |
| `radarDelayS` | 1 | 1..3600 | Seconds between distance readings |
| `medianWindow` | 5 | 1..15 | Readings used for the median |
| `pumpMinRunS` | 10 | 0..600 | Minimum seconds ON before a stop request is accepted |
| `pumpMinRestartS` | 10 | 0..600 | Minimum seconds OFF before the next start |
| `pumpMaxTimeOnM` | 20 | 1..240 | Longest continuous run (minutes), then forced stop + cooldown |
| `pumpCooldownTimeM` | 60 | 1..1440 | Lockout after a max-on-time stop (minutes) |
| `nightStartHour` | 22 | 0..23 | Night starts (local hour) |
| `nightEndHour` | 8 | 0..23 | Night ends (local hour) |
| `utcOffsetMinutes` | -180 | -720..840 | Local time = UTC + this offset (minutes, no daylight saving). -180 = UTC-3 |
| `watchdogDelayS` | 60 | 10..3600 | Seconds between network checks (ping to the router) |
| `maxReconnects` | 10 | 1..100 | Failed reconnect rounds before the device restarts |

Rule between settings: `criticalLevelCm < minLevelCm < maxLevelCm <= heightCm`.
The table in `src/core/Settings.cpp` is the source of truth; this table must match it.

## Error codes

| Code | Meaning |
|---|---|
| `BAD_REQUEST` | Not valid JSON, not an object, missing/invalid `id` or `cmd`, wrong field types, empty or oversized data |
| `UNKNOWN_COMMAND` | `cmd` is not a known command |
| `UNKNOWN_FIELD` | A setting name does not exist (or is too long) |
| `INVALID_VALUE` | A setting value is not an integer, is out of range, or breaks the rule between settings |
| `PUMP_COOLDOWN` | Pump is locked out after running too long |
| `PUMP_TOO_SOON` | Start/stop refused to avoid rapid cycling |
| `HIGH_LEVEL` | Tank is full, the pump will not start |
| `PUMP_FAULT` | Pump is in fault (clock not synced, or inconsistent level sensors) |
| `INTERNAL` | Unexpected device error |

## Trying it from Windows
`tools/ws-check.ps1` connects to `/ws/v1`, sends `state.get` and `settings.get`, and prints every message the
device sends (answers and pushed events) for a few seconds. No extra tools are needed.
```powershell
.\tools\ws-check.ps1 -Ip 192.168.100.247 -Seconds 30
.\tools\ws-check.ps1 -Send '{"cmd":"pump.set","id":5,"data":{"on":false}}'
```
Make a REST call from another window while it runs to see the events it causes.

## Logging
On the serial console (tag `WS`, Info level) the device logs clients connecting and disconnecting, every command
it receives and every answer it sends (long answers are cut after 100 characters). Pushed events are not logged.
```
[I][WS] Client #3 connected from 192.168.100.5
[I][WS] WS #3 received: {"cmd":"state.get","id":1}
[I][WS] WS #3 answered: {"id":1,"ok":true,"data":{"pump":{"mode":"idle","on":false,"cooldown":false,"fault":"none",...
```

## Compatibility rules
- Adding a new command, event, field or error code does not change the version. Clients must ignore what they do not know.
- Renaming or removing anything, or changing a meaning, requires a new version (new endpoint path).
- Changes to this document need explicit approval (see `constitution.md`).
