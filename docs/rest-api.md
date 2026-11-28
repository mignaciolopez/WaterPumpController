# WaterPumpController REST API v1

Plain HTTP + JSON, on the same port as the WebSocket (80 by default). The version is in the path: `/api/v1/`.
It offers the same data and the same commands as the WebSocket protocol (`docs/protocol.md`), as ordinary
request/response calls. Nothing is pushed to the client; for live updates use the WebSocket.

## Endpoints

| Method | Path | Body | Purpose |
|---|---|---|---|
| `GET` | `/api/v1/state` | - | Pump state and water level |
| `GET` | `/api/v1/settings` | - | All settings |
| `POST` | `/api/v1/pump` | `{"on":true}` | Switch the pump on or off |
| `PUT` | `/api/v1/settings` | `{"minLevelCm":90,...}` | Change one or more settings (atomic) |

Write requests (`POST`, `PUT`) must send `Content-Type: application/json` and a body of at most 512 bytes.

### GET /api/v1/state
```
$ curl http://<ip>/api/v1/state
{"pump":{"mode":"idle","on":false,"cooldown":false,"fault":"none","onAt":0,"offAt":0,"cooldownAt":0},"level":{"cm":45.2},"sensor":{"minDistanceCm":3,"maxDistanceCm":450}}
```
`level.cm` is `null` while the level is unknown. The fields are the same as in the WebSocket `pump.state`
and `tank.level` events (see `docs/protocol.md`).

The `sensor` block says what the distance sensor can measure, so a client can scale its display or validate
input: `minDistanceCm` (the closest it can measure, 3 for the A02YYUW) and `maxDistanceCm` (the farthest, 450). It
describes the **distance** from the sensor to the water. The level is `heightCm - distance`, so it can range from
`max(0, heightCm - maxDistanceCm)` up to `heightCm - minDistanceCm` (137 for the default height of 140). The block is
only present when the device knows the sensor's range, and it is not repeated in the pushed events.

### GET /api/v1/settings
```
$ curl http://<ip>/api/v1/settings
{"heightCm":140,"criticalLevelCm":20,"minLevelCm":50, ...}
```
Names, ranges and meanings are the table in `docs/protocol.md`.

### POST /api/v1/pump
```
$ curl -X POST http://<ip>/api/v1/pump -H "Content-Type: application/json" -d '{"on":true}'
{"ok":true}
```
The same safety rules as everywhere apply: a start can be refused (cooldown, too soon, tank full, fault) and
a stop right after a start is refused as too soon.

### PUT /api/v1/settings
```
$ curl -X PUT http://<ip>/api/v1/settings -H "Content-Type: application/json" -d '{"minLevelCm":90,"maxLevelCm":100}'
{"ok":true}
```
All values are applied together or not at all. Values must be integers.

## Windows PowerShell
In Windows PowerShell `curl` is an alias for `Invoke-WebRequest` and does not accept the curl options used
above. Use `Invoke-RestMethod`:
```powershell
Invoke-RestMethod http://<ip>/api/v1/state
Invoke-RestMethod -Method Post -Uri http://<ip>/api/v1/pump -ContentType 'application/json' -Body '{"on":true}'
Invoke-RestMethod -Method Put -Uri http://<ip>/api/v1/settings -ContentType 'application/json' -Body '{"minLevelCm":60}'
```
PowerShell throws on 4xx/5xx answers. To see the status code and the error body:
```powershell
try { Invoke-RestMethod -Method Put -Uri http://<ip>/api/v1/settings -ContentType 'application/json' -Body '{"medianWindow":99}' }
catch { $_.Exception.Response.StatusCode.value__; $_.ErrorDetails.Message }
```

## Answers and errors

Success is HTTP `200` with `{"ok":true}` (or the data for the `GET` calls). A refused request has an HTTP
error status and a body `{"ok":false,"err":"<CODE>"}`. The codes are the same as in `docs/protocol.md`.

| HTTP status | `err` | Meaning |
|---|---|---|
| 400 | `BAD_REQUEST` | Body missing, too large, not valid JSON, or wrong field types |
| 400 | `UNKNOWN_FIELD` | A setting name does not exist |
| 415 | `BAD_REQUEST` | `Content-Type` is not `application/json` |
| 405 | `BAD_REQUEST` | Wrong method for an existing path (for example `GET /api/v1/pump`) |
| 422 | `INVALID_VALUE` | Not an integer, out of range, or breaks the rule between settings |
| 409 | `PUMP_COOLDOWN`, `PUMP_TOO_SOON`, `HIGH_LEVEL`, `PUMP_FAULT` | The pump refused the request |
| 500 | `INTERNAL` | Device error, queue full, or no answer within 2 seconds |

Unknown paths answer `404` with `{"ok":false,"err":"UNKNOWN_COMMAND"}`.

## How it behaves
- It runs on the ESP-IDF HTTP server (`esp_http_server`) in its own task, shared with the WebSocket.
- `GET` answers from a copy that is refreshed whenever the state or the settings change (at most 250 ms old).
  It never waits for the pump logic.
- `POST` and `PUT` go through the same command queue as the WebSocket, so only one task ever changes the
  pump state. The call waits up to 2 seconds for the result. A change made over REST is also announced to
  WebSocket clients as events.
- After a successful `POST`/`PUT`, a `GET` already shows the new state.

## Logging
Every call is logged on the serial console (tag `REST`, Info level): one line when it arrives and one when it
is answered. Writes also show the request body, and any answer other than a successful read shows the answer body.
```
[I][REST] GET /api/v1/state from 192.168.100.5
[I][REST] GET /api/v1/state -> 200
[I][REST] POST /api/v1/pump from 192.168.100.5 body: {"on":true}
[I][REST] POST /api/v1/pump -> 409 {"ok":false,"err":"HIGH_LEVEL"}
[I][REST] GET /api/v1/nothing -> 404 {"ok":false,"err":"UNKNOWN_COMMAND"}
```

## Security
There is **no authentication**: anyone who can reach the device on the network can read the state and
control the pump. Run it only on a network you trust, and do not expose port 80 to the internet.

Write requests require `Content-Type: application/json`. A web page on another site cannot send that with a
simple cross-site request, and the device does not allow cross-origin requests (no CORS headers), so browsers
refuse them. This does not protect against tools like `curl` or other devices on the network.

## Compatibility rules
- Adding endpoints, fields or error codes does not change the version. Clients must ignore what they do not know.
- Renaming or removing anything, or changing a meaning, requires a new path version (`/api/v2/`).
- Changes to this document need explicit approval (see `constitution.md`).
