<#
.SYNOPSIS
  Connects to and disconnects from the WebSocket many times, at random moments, to provoke a crash
  that only happens now and then (a race between sending data and a client closing).

.DESCRIPTION
  Each cycle: connect to ws://<Ip>/ws/v1, send state.get, listen for a random time (so the close lands at a
  random point between the device's pushed events), then close. Every second cycle closes abruptly (the
  connection is dropped without a close handshake) when -Abrupt is given.
  After the cycles it asks the REST API whether the board is still alive. If the board stops answering
  (it crashed and is rebooting) the script says so and stops: look at the serial console.
  Works in Windows PowerShell 5.1 and PowerShell 7.

.EXAMPLE
  .\tools\ws-stress.ps1
  .\tools\ws-stress.ps1 -Cycles 100 -Abrupt
#>
param(
    [string]$Ip = '192.168.100.247',
    [int]$Cycles = 40,
    [int]$MinHoldMs = 200,
    [int]$MaxHoldMs = 3000,
    [switch]$Abrupt
)

$uri = [Uri]"ws://$Ip/ws/v1"
$none = [Threading.CancellationToken]::None
$command = [Text.Encoding]::UTF8.GetBytes('{"cmd":"state.get","id":1}')
$done = 0
$failuresInARow = 0
$crashed = $false

# Tells the three ways a board can stop answering apart. Call it after a short wait.
function Show-BoardDiagnosis {
    Start-Sleep -Seconds 3
    $pings = Test-Connection -ComputerName $Ip -Count 3 -Quiet -ErrorAction SilentlyContinue
    if ($pings) {
        Write-Host 'The board still answers ping, but not HTTP: the web server stopped answering (the board did not reboot).' -ForegroundColor Red
    } else {
        Write-Host 'The board does not answer ping either: it crashed and is rebooting, or it lost WiFi. Look for CORRUPT HEAP, Guru Meditation or STA_DISCONNECTED on the serial console.' -ForegroundColor Red
    }
}

function Test-Board {
    try { Invoke-RestMethod -Uri "http://$Ip/api/v1/state" -TimeoutSec 5 | Out-Null; return $true } catch { return $false }
}

Write-Host "Stress test: $Cycles connect/disconnect cycles against $uri$(if ($Abrupt) { ' (every 2nd one abrupt)' })"

for ($i = 1; $i -le $Cycles; $i++) {
    $ws = New-Object Net.WebSockets.ClientWebSocket
    $hold = Get-Random -Minimum $MinHoldMs -Maximum ($MaxHoldMs + 1)
    $how = 'closed'
    try {
        $ws.ConnectAsync($uri, $none).Wait(5000) | Out-Null
        if ($ws.State -ne 'Open') { throw "could not connect (state $($ws.State))" }

        $segment = New-Object ArraySegment[byte] -ArgumentList (, $command)
        $ws.SendAsync($segment, [Net.WebSockets.WebSocketMessageType]::Text, $true, $none).Wait()

        # Read what arrives while holding the connection, like a real client.
        $buffer = New-Object byte[] 8192
        $receive = New-Object ArraySegment[byte] -ArgumentList (, $buffer)
        $pending = $null
        $until = (Get-Date).AddMilliseconds($hold)
        while ((Get-Date) -lt $until -and $ws.State -eq 'Open') {
            if ($null -eq $pending) { $pending = $ws.ReceiveAsync($receive, $none) }
            if ($pending.Wait(50)) { $pending = $null }
        }

        if ($Abrupt -and ($i % 2 -eq 0)) {
            $ws.Abort()
            $how = 'dropped'
        } else {
            $ws.CloseAsync([Net.WebSockets.WebSocketCloseStatus]::NormalClosure, 'bye', $none).Wait(2000) | Out-Null
        }
        $done++
        $failuresInARow = 0
        Write-Host ("cycle {0,3}/{1}: held {2,4} ms, {3}" -f $i, $Cycles, $hold, $how)
    } catch {
        $failuresInARow++
        Write-Host ("cycle {0,3}/{1}: FAILED - {2}" -f $i, $Cycles, $_.Exception.GetBaseException().Message) -ForegroundColor Red
        if ($failuresInARow -ge 2 -and -not (Test-Board)) { $crashed = $true; break }
        Start-Sleep -Seconds 3
    } finally {
        $ws.Dispose()
    }
    Start-Sleep -Milliseconds (Get-Random -Minimum 100 -Maximum 700)
}

Write-Host ''
if ($crashed) {
    Write-Host "The board stopped answering after $done good cycles." -ForegroundColor Red
    Show-BoardDiagnosis
    exit 2
}
if (Test-Board) {
    Write-Host "Finished $done of $Cycles cycles and the board still answers." -ForegroundColor Green
} else {
    Write-Host "Finished $done of $Cycles cycles but the board does NOT answer now." -ForegroundColor Red
    Show-BoardDiagnosis
    exit 2
}
