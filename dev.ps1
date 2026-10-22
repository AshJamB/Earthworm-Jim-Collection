# Developer helper for the Windows build.
# Usage (PowerShell, from the repo root):
#   .\dev.ps1 build        incremental build
#   .\dev.ps1 rebuild      delete build\ and build from scratch
#   .\dev.ps1 run          copy the USA ROMs from roms\ into build\bin\roms\ and start the exe
#   .\dev.ps1 reset        delete settings, achievements and save states (build\bin and %LOCALAPPDATA%)
#   .\dev.ps1 all          rebuild, reset, run
param([Parameter(Position = 0)][ValidateSet('build', 'rebuild', 'run', 'reset', 'all')][string]$Action = 'build')

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$exe  = Join-Path $root 'build\bin\EarthwormJimCollection.exe'

function Build {
    Push-Location $root
    try {
        cmd /c build.bat 2>&1 | Tee-Object (Join-Path $root 'build-log.txt') |
            Select-String 'error|warning C|Build finished|Build failed'
        if (-not (Test-Path $exe)) { throw 'Build failed: see build-log.txt' }
    } finally { Pop-Location }
}

function Rebuild {
    $b = Join-Path $root 'build'
    if (Test-Path $b) { Remove-Item $b -Recurse -Force }
    Build
}

function Reset {
    $dirs = @((Join-Path $root 'build\bin'), (Join-Path $env:LOCALAPPDATA 'Earthworm Jim Collection'))
    foreach ($d in $dirs) {
        if (-not (Test-Path $d)) { continue }
        foreach ($f in 'settings.ini', 'achievements.ini') {
            $p = Join-Path $d $f
            if (Test-Path $p) { Remove-Item $p -Force; Write-Host "deleted $p" }
        }
        foreach ($sub in 'saves', 'screenshots') {
            $p = Join-Path $d $sub
            if (Test-Path $p) { Remove-Item $p -Recurse -Force; Write-Host "deleted $p" }
        }
    }
}

function Run {
    if (-not (Test-Path $exe)) { throw 'No exe yet: run .\dev.ps1 build first' }
    $dest = Join-Path $root 'build\bin\roms'
    New-Item -ItemType Directory -Force $dest | Out-Null
    Get-ChildItem (Join-Path $root 'roms') -Filter '*(USA)*.md' | Copy-Item -Destination $dest -Force
    Start-Process $exe -WorkingDirectory (Join-Path $root 'build\bin')
}

switch ($Action) {
    'build'   { Build }
    'rebuild' { Rebuild }
    'run'     { Run }
    'reset'   { Reset }
    'all'     { Rebuild; Reset; Run }
}
