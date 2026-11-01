<#
.SYNOPSIS
    Monitor KhaiRC ESP32 Hub via read-only REST APIs and save CSV samples.
.EXAMPLE
    .\tools\monitor-hub.ps1 -HubIP "192.168.1.105" -DurationMinutes 10
.EXAMPLE
    .\tools\monitor-hub.ps1 -HubIP "192.168.1.105" -DurationMinutes 1440 -IntervalSeconds 60
.NOTES
    Compatible with Windows PowerShell 5.1 and PowerShell 7.
    Does not send or record any API key.
#>

param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$HubIP,

    [ValidateRange(1, 10080)]
    [int]$DurationMinutes = 10,

    [ValidateRange(5, 3600)]
    [int]$IntervalSeconds = 60,

    [ValidateRange(1, 60)]
    [int]$TimeoutSeconds = 5
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$baseUrl = 'http://' + $HubIP.Trim().TrimEnd('/')
$projectRoot = Split-Path -Parent $PSScriptRoot
$logDir = Join-Path $projectRoot 'logs'
New-Item -Path $logDir -ItemType Directory -Force | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$csvPath = Join-Path $logDir ("hub-monitor-$stamp.csv")

$durationSeconds = [double]$DurationMinutes * 60.0
$sampleCount = [int][Math]::Ceiling($durationSeconds / $IntervalSeconds)
$successful = 0
$failures = 0
$detectedResets = 0
$previousUptime = $null
$minHeap = $null
$maxHeap = $null

Write-Host '=== KhaiRC ESP32 Hub - Stability Monitor ==='
Write-Host "Hub: $baseUrl"
Write-Host "Duration: $DurationMinutes minute(s)"
Write-Host "Interval: $IntervalSeconds second(s)"
Write-Host "Samples planned: $sampleCount"
Write-Host "CSV: $csvPath"
Write-Host 'Use Ctrl+C to stop early.'
Write-Host ''

$clock = [System.Diagnostics.Stopwatch]::StartNew()

try {
    for ($i = 0; $i -lt $sampleCount; $i++) {
        # Schedule samples relative to start time, rather than accumulating
        # the time taken by HTTP requests into the interval.
        $scheduledMs = [double]$i * $IntervalSeconds * 1000.0
        $sleepMs = [int][Math]::Ceiling($scheduledMs - $clock.Elapsed.TotalMilliseconds)
        if ($sleepMs -gt 0) {
            Start-Sleep -Milliseconds $sleepMs
        }

        if ($clock.Elapsed.TotalSeconds -ge $durationSeconds) {
            break
        }

        $sampleTime = Get-Date
        $status = 'OK'
        $uptime = $null
        $heap = $null
        $rssi = $null
        $deviceIp = $null
        $errorMessage = ''

        try {
            $health = Invoke-RestMethod -Uri "$baseUrl/api/v1/health" `
                -Method Get -TimeoutSec $TimeoutSeconds -ErrorAction Stop
            $system = Invoke-RestMethod -Uri "$baseUrl/api/v1/system" `
                -Method Get -TimeoutSec $TimeoutSeconds -ErrorAction Stop

            if ($health.status -ne 'ok') {
                throw 'Health endpoint did not return status=ok.'
            }
            if ($null -eq $health.uptime_sec -or
                $null -eq $health.free_heap -or
                $null -eq $system.rssi_dbm) {
                throw 'A required health/system metric is missing.'
            }

            $uptime = [long]$health.uptime_sec
            $heap = [long]$health.free_heap
            $rssi = [int]$system.rssi_dbm
            $deviceIp = [string]$system.ip

            if ($uptime -lt 0 -or $heap -lt 0) {
                throw 'Invalid negative uptime or heap size.'
            }

            if ($null -ne $previousUptime -and $uptime -lt $previousUptime) {
                $detectedResets++
                $status = 'RESET'
            }
            $previousUptime = $uptime
            $successful++

            if ($null -eq $minHeap -or $heap -lt $minHeap) { $minHeap = $heap }
            if ($null -eq $maxHeap -or $heap -gt $maxHeap) { $maxHeap = $heap }
        }
        catch {
            $status = 'ERROR'
            $errorMessage = $_.Exception.Message
            $failures++
        }

        [PSCustomObject]@{
            Time = $sampleTime.ToString('o')
            Status = $status
            UptimeSec = $uptime
            FreeHeapBytes = $heap
            RSSIdBm = $rssi
            IPAddress = $deviceIp
            Error = $errorMessage
        } | Export-Csv -Path $csvPath -Append -NoTypeInformation -Encoding UTF8

        $heapText = if ($null -eq $heap) { '-' } else { [string]$heap }
        $rssiText = if ($null -eq $rssi) { '-' } else { [string]$rssi }
        $uptimeText = if ($null -eq $uptime) { '-' } else { [string]$uptime }
        Write-Host ("{0} [{1}/{2}] {3} uptime={4}s heap={5}B rssi={6}dBm" -f `
            $sampleTime.ToString('yyyy-MM-dd HH:mm:ss'), `
            ($i + 1), $sampleCount, $status, $uptimeText, $heapText, $rssiText)

        if ($errorMessage) {
            Write-Warning $errorMessage
        }
    }

    # Honor the full requested monitoring duration, including the final
    # interval following the last sample.
    $remainingMs = [int][Math]::Ceiling(
        ($durationSeconds - $clock.Elapsed.TotalSeconds) * 1000.0
    )
    if ($remainingMs -gt 0) {
        Start-Sleep -Milliseconds $remainingMs
    }
}
finally {
    $clock.Stop()
    Write-Host ''
    Write-Host '=== Monitoring summary ==='
    Write-Host "Successful samples: $successful"
    Write-Host "Failed samples: $failures"
    Write-Host "Detected uptime drops (possible resets): $detectedResets"
    Write-Host "Minimum free heap: $minHeap bytes"
    Write-Host "Maximum free heap: $maxHeap bytes"
    Write-Host ("Elapsed: {0:N1} minutes" -f $clock.Elapsed.TotalMinutes)
    Write-Host "CSV: $csvPath"
    Write-Host 'Note: uptime drops are a heuristic; resets occurring during long gaps may go undetected.'
}
