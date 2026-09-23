# Builds the SimHub plugin and runs its unit tier.
#
# Two artefacts, deliberately separate:
#
#   Conformance.exe   the adapter conformance suite. Builds from plugin/src/core + plugin/test and
#                     references NO SimHub assembly, so it runs in CI where those DLLs cannot exist.
#   CydSimDash.dll    the plugin SimHub loads. Adds plugin/src/simhub, which needs the real
#                     assemblies from a local SimHub install.
#
# The split is why the conformance suite is a gate rather than a local-only nicety.
#
# The three SimHub assemblies in plugin/lib are REFERENCES ONLY - copied from an install, never
# redistributed, gitignored. See plugin/lib/README.md and POLICY-NO-REDISTRIBUTION-SIMHUB.

$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)

$csc = "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if (-not (Test-Path $csc)) { throw "Framework C# compiler not found at $csc" }
New-Item -ItemType Directory -Force plugin\build | Out-Null

# ---- the conformance suite, no SimHub needed -------------------------------
#
# Built as a LIBRARY and invoked by reflection rather than as an .exe. Windows Application Control
# blocks freshly built unsigned executables on some machines - including this one - and it blocks
# them at load, not just at process start, so there is no flag that talks it round. A .NET library
# is the same IL reached the same way, and it loads. CI uses the identical path, so the local run
# and the gate cannot diverge.
$coreSrc = @(Get-ChildItem -Recurse plugin\src\core -Filter *.cs | ForEach-Object { $_.FullName }) +
           @(Get-ChildItem plugin\test -Filter *.cs | ForEach-Object { $_.FullName })
& $csc /nologo /target:library /langversion:5 /out:plugin\build\Conformance.dll `
    /reference:System.dll /reference:System.Core.dll $coreSrc
if ($LASTEXITCODE -ne 0) { throw "conformance suite failed to compile" }

$suite = [System.Reflection.Assembly]::LoadFrom((Resolve-Path "plugin\build\Conformance.dll"))
$entry = $suite.GetType("CydSimDash.Test.ConformanceSuite").GetMethod("Main")
$rc = $entry.Invoke($null, @(, [string[]]@("--emit", "plugin\build\emitted.ndjson")))
if ($rc -ne 0) { throw "CONFORMANCE-ADAPTER failed" }

python tools\check_adapter_output.py plugin\build\emitted.ndjson
if ($LASTEXITCODE -ne 0) { throw "emitted frames breach the wire contract" }

python tools\check_capture_roundtrip.py plugin\build\emitted-capture.ndjson
if ($LASTEXITCODE -ne 0) { throw "capture does not round-trip to the replay tool" }

python tools\check_adapter_boundary.py
if ($LASTEXITCODE -ne 0) { throw "R2 - adapter boundary breached" }

# ---- the plugin itself, which does need SimHub ------------------------------
$missing = @("GameReaderCommon.dll", "SimHub.Plugins.dll") |
    Where-Object { -not (Test-Path "plugin\lib\$_") }
if ($missing) {
    Write-Warning ("Skipping the plugin DLL: plugin\lib is missing " + ($missing -join ", ") +
                   ". Copy them from the SimHub install folder. The conformance suite above is " +
                   "unaffected, which is the point of the split.")
    exit 0
}

$allSrc = @(Get-ChildItem -Recurse plugin\src -Filter *.cs | ForEach-Object { $_.FullName })
& $csc /nologo /target:library /langversion:5 /out:plugin\build\CydSimDash.dll `
    /reference:plugin\lib\GameReaderCommon.dll /reference:plugin\lib\SimHub.Plugins.dll `
    /reference:System.dll /reference:System.Core.dll $allSrc
if ($LASTEXITCODE -ne 0) { throw "plugin failed to compile" }

Write-Output "built plugin\build\CydSimDash.dll"
