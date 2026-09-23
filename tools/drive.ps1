# Drive the panel with one scenario, and exactly one.
#
# This exists because a second replay is almost invisible and very confusing. Two senders with
# independent stamp bases each look correct on their own; together the device rejects roughly half
# of everything as out-of-order, the panel jumps between two scenarios, and the end-to-end suite
# fails checks that have nothing wrong with them. It cost three separate misdiagnoses before the
# operator spotted it on the glass.
#
# The trap underneath is that `pkill -f replay.py` from Git Bash, and `kill $PID` on a process
# started with `&`, do NOT stop a native Windows Python process. They report success and the
# process keeps sending. Stop-Process does work, so everything goes through here.
#
# Usage:
#   tools/drive.ps1 gears
#   tools/drive.ps1 sweep -Device 192.168.50.230 -Seconds 300
#   tools/drive.ps1 -StopOnly

param(
    [Parameter(Position = 0)][string] $Scenario = "sweep",
    [string] $Device = "cyd-sim-dash.local",
    [int]    $Seconds = 300,
    [switch] $StopOnly
)

$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)

# Always clear the field first, however this was invoked.
$existing = Get-CimInstance Win32_Process -Filter "Name like '%python%'" |
            Where-Object { $_.CommandLine -like '*replay.py*' }
foreach ($proc in $existing) {
    Stop-Process -Id $proc.ProcessId -Force
    Write-Output "stopped a replay already running (PID $($proc.ProcessId))"
}

if ($StopOnly) {
    Write-Output "nothing is driving the panel now"
    exit 0
}

Start-Sleep -Milliseconds 400
$job = Start-Process -FilePath "python" `
    -ArgumentList @("tools/replay.py", "synth", "--host", $Device, "--scenario", $Scenario, "--loop") `
    -PassThru -WindowStyle Hidden

Write-Output "driving $Device with '$Scenario' for $Seconds s  (PID $($job.Id))"
Write-Output "stop early with: tools/drive.ps1 -StopOnly"

Start-Sleep -Seconds $Seconds
if (-not $job.HasExited) { Stop-Process -Id $job.Id -Force }
Write-Output "finished"
