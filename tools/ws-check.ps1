<#
.SYNOPSIS
  Quick check of the WaterPumpController WebSocket (protocol v1, see docs/protocol.md).

.DESCRIPTION
  Connects to ws://<Ip>/ws/v1, sends the commands in -Send, then prints every message the device sends
  for -Seconds seconds (answers and pushed events such as pump.state and tank.level).
  While it runs, make a REST call from another PowerShell window (for example POST /api/v1/pump) to
  confirm that the events caused by REST reach the WebSocket.
  Works in Windows PowerShell 5.1 and PowerShell 7. Needs no extra tools.

.EXAMPLE
  .\tools\ws-check.ps1
  .\tools\ws-check.ps1 -Ip 192.168.100.247 -Seconds 30
  .\tools\ws-check.ps1 -Send '{"cmd":"pump.set","id":5,"data":{"on":false}}'
#>
param(
    [string]$Ip = '192.168.100.247',
    [int]$Seconds = 15,
    [string[]]$Send = @('{"cmd":"state.get","id":1}', '{"cmd":"settings.get","id":2}')
)

$uri = [Uri]"ws://$Ip/ws/v1"
$none = [Threading.CancellationToken]::None
$ws = New-Object Net.WebSockets.ClientWebSocket

function Show([string]$arrow, [string]$text) {
    $shown = $text
    if ($shown.Length -gt 300) { $shown = $shown.Substring(0, 300) + '...' }
    Write-Host ("{0} {1} {2}" -f (Get-Date -Format 'HH:mm:ss.fff'), $arrow, $shown)
}

Write-Host "Connecting to $uri ..."
try {
    $ws.ConnectAsync($uri, $none).Wait(5000) | Out-Null
} catch {
    Write-Host "FAILED to connect: $($_.Exception.GetBaseException().Message)" -ForegroundColor Red
    exit 1
}
if ($ws.State -ne 'Open') {
    Write-Host "FAILED to connect (state: $($ws.State))" -ForegroundColor Red
    exit 1
}
Write-Host "Connected. Listening for $Seconds seconds (make a REST call now to see its events)." -ForegroundColor Green

foreach ($text in $Send) {
    $bytes = [Text.Encoding]::UTF8.GetBytes($text)
    $segment = New-Object ArraySegment[byte] -ArgumentList (, $bytes)
    $ws.SendAsync($segment, [Net.WebSockets.WebSocketMessageType]::Text, $true, $none).Wait()
    Show '->' $text
}

$buffer = New-Object byte[] 8192
$segment = New-Object ArraySegment[byte] -ArgumentList (, $buffer)
$message = New-Object Text.StringBuilder
$pending = $null
$answers = 0
$events = @{}
$deadline = (Get-Date).AddSeconds($Seconds)

while ((Get-Date) -lt $deadline -and $ws.State -eq 'Open') {
    # One receive is kept running and polled; cancelling it would close the socket.
    if ($null -eq $pending) { $pending = $ws.ReceiveAsync($segment, $none) }
    if (-not $pending.Wait(250)) { continue }

    $result = $pending.Result
    $pending = $null
    if ($result.MessageType -eq [Net.WebSockets.WebSocketMessageType]::Close) { break }

    [void]$message.Append([Text.Encoding]::UTF8.GetString($buffer, 0, $result.Count))
    if (-not $result.EndOfMessage) { continue }

    $text = $message.ToString()
    [void]$message.Clear()
    Show '<-' $text

    if ($text -match '^\{"id":\d+,"ok":') { $answers++ }
    if ($text -match '^\{"event":"([a-z.]+)"') {
        $name = $Matches[1]
        if ($events.ContainsKey($name)) { $events[$name]++ } else { $events[$name] = 1 }
    }
}

try { $ws.CloseAsync([Net.WebSockets.WebSocketCloseStatus]::NormalClosure, 'bye', $none).Wait(2000) | Out-Null } catch { }
$ws.Dispose()

Write-Host ''
Write-Host "Answers to commands: $answers (sent $($Send.Count))"
if ($events.Count -eq 0) { Write-Host 'Events: none' }
foreach ($name in $events.Keys) { Write-Host "Events: $name x $($events[$name])" }
if ($answers -ge $Send.Count) {
    Write-Host 'OK: every command was answered.' -ForegroundColor Green
} else {
    Write-Host 'WARNING: fewer answers than commands.' -ForegroundColor Yellow
}
