param([string]$LegacyRoot = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$pythonExe = Join-Path $projectRoot '.venv-personal\Scripts\python.exe'
if (!(Test-Path -LiteralPath $pythonExe)) { throw 'Missing .venv-personal. See docs/personal-upstream.md.' }
$launchArgs = @((Join-Path $PSScriptRoot 'start-personal.py'))
if ($LegacyRoot) { $launchArgs += @('--legacy-root', $LegacyRoot) }
& $pythonExe @launchArgs
if ($LASTEXITCODE -ne 0) { throw "Personal workspace exited with code $LASTEXITCODE" }
