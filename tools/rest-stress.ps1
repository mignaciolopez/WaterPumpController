<#
.SYNOPSIS
  Sends many REST requests to the device, quickly and in random order, to find problems that only show up
  under load (memory corruption, dropped connections).

.DESCRIPTION
  Mixes: GET state, GET settings, a PUT that sets a setting to the value it already has (nothing changes),
  an unknown path (expects 404) and a write with the wrong Content-Type (expects 415). The pump is never
  switched. Checks every status code, then asks the board whether it is still alive.
  Run it together with tools\ws-stress.ps1 in another window to stress both interfaces at once.
  Works in Windows PowerShell 5.1 and PowerShell 7.

.EXAMPLE
  .\tools\rest-stress.ps1
  .\tools\rest-stress.ps1 -Requests 500 -DelayMs 0
#>
param(
    [string]$Ip = '192.168.100.247',
    [int]$Requests = 300,
    [int]$DelayMs = 30
)

$base = "http://$Ip/api/v1"

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

function Invoke-Status([string]$method, [string]$path, [string]$body, [string]$contentType) {
    try {
        $params = @{ Uri = "$base$path"; Method = $method; UseBasicParsing = $true; TimeoutSec = 8 }
        if ($body) { $params.Body = $body; $params.ContentType = $contentType }
        return [int](Invoke-WebRequest @params).StatusCode
    } catch {
        if ($_.Exception.Response) { return [int]$_.Exception.Response.StatusCode }
        return -1   # no answer at all
    }
}

# Current minLevelCm, so the PUT writes back the very same value.
$minLevel = 50
try { $minLevel = [int](Invoke-RestMethod -Uri "$base/settings" -TimeoutSec 8).minLevelCm } catch { }

$cases = @(
    @{ Name = 'GET state';      Method = 'GET'; Path = '/state';    Body = $null; Type = $null;                Expect = 200 },
    @{ Name = 'GET settings';   Method = 'GET'; Path = '/settings'; Body = $null; Type = $null;                Expect = 200 },
    @{ Name = 'PUT same value'; Method = 'PUT'; Path = '/settings'; Body = "{`"minLevelCm`":$minLevel}"; Type = 'application/json'; Expect = 200 },
    @{ Name = 'GET unknown';    Method = 'GET'; Path = '/nothing';  Body = $null; Type = $null;                Expect = 404 },
    @{ Name = 'PUT wrong type'; Method = 'PUT'; Path = '/settings'; Body = "{`"minLevelCm`":$minLevel}"; Type = 'text/plain'; Expect = 415 }
)

$bad = 0
$noAnswer = 0
Write-Host "REST stress test: $Requests requests against $base"

for ($i = 1; $i -le $Requests; $i++) {
    $case = $cases | Get-Random
    $status = Invoke-Status $case.Method $case.Path $case.Body $case.Type

    if ($status -eq $case.Expect) { continue }

    $bad++
    if ($status -eq -1) { $noAnswer++ } else { $noAnswer = 0 }
    Write-Host ("request {0,4}/{1}: {2} answered {3}, expected {4}" -f $i, $Requests, $case.Name, $status, $case.Expect) -ForegroundColor Red
    if ($noAnswer -ge 3) {
        Write-Host 'The board stopped answering.' -ForegroundColor Red
        Show-BoardDiagnosis
        exit 2
    }
    if ($status -eq -1) { Start-Sleep -Seconds 2 }

    if ($DelayMs -gt 0) { Start-Sleep -Milliseconds $DelayMs }
}

Write-Host ''
$alive = (Invoke-Status 'GET' '/state' $null $null) -eq 200
if ($bad -eq 0 -and $alive) {
    Write-Host "All $Requests requests answered as expected and the board is alive." -ForegroundColor Green
} elseif ($alive) {
    Write-Host "$bad of $Requests requests answered unexpectedly (see above); the board is still alive." -ForegroundColor Yellow
} else {
    Write-Host "The board does NOT answer now." -ForegroundColor Red
    Show-BoardDiagnosis
    exit 2
}
