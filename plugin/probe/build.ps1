# Builds the telemetry probe.
#
# Deliberately uses the .NET Framework compiler shipped with Windows rather than the .NET SDK:
# SimHub plugins target .NET Framework 4.8, and this keeps the probe buildable on a machine with
# no SDK installed. The cost is C# 5 — no interpolated strings, no null-conditionals.
#
# The three SimHub assemblies in plugin\lib\ are REFERENCES ONLY. They are copied from a SimHub
# installation, are not ours to redistribute, and are gitignored. See plugin\lib\README.md.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

$csc = "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if (-not (Test-Path $csc)) { throw "Framework C# compiler not found at $csc" }

foreach ($dll in @("GameReaderCommon.dll", "SimHub.Plugins.dll")) {
    if (-not (Test-Path "plugin\lib\$dll")) {
        throw "Missing reference assembly plugin\lib\$dll - copy it from the SimHub install folder"
    }
}

New-Item -ItemType Directory -Force plugin\build | Out-Null
& $csc /nologo /target:library /langversion:5 `
    /out:plugin\build\CydSimDashProbe.dll `
    /reference:plugin\lib\GameReaderCommon.dll `
    /reference:plugin\lib\SimHub.Plugins.dll `
    /reference:System.dll /reference:System.Core.dll `
    plugin\probe\ProbePlugin.cs
if ($LASTEXITCODE -ne 0) { throw "compile failed" }

# The distributable: the DLL beside the instructions, zipped for the trip to the rig.
New-Item -ItemType Directory -Force plugin\dist\cyd-probe | Out-Null
Copy-Item plugin\build\CydSimDashProbe.dll plugin\dist\cyd-probe\ -Force
if (Test-Path plugin\dist\cyd-probe.zip) { Remove-Item plugin\dist\cyd-probe.zip -Force }
Compress-Archive -Path "plugin\dist\cyd-probe\*" -DestinationPath plugin\dist\cyd-probe.zip

Write-Output "built plugin\dist\cyd-probe.zip"
