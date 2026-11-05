$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$pythonExe = Join-Path $projectRoot '.venv-gpu\Scripts\python.exe'
$nextCli = Join-Path $projectRoot 'apps\web\node_modules\next\dist\bin\next'
$mediaBin = Join-Path $projectRoot 'data\tools\ffmpeg\bin'
$logDir = Join-Path $projectRoot 'data\launcher'

if (!(Test-Path -LiteralPath $pythonExe)) { throw 'Missing .venv-gpu. See docs/local-gpu-runtime.md.' }
if (!(Test-Path -LiteralPath $nextCli)) { throw 'Missing frontend dependencies. Run npm ci in apps/web.' }
if (!(Test-Path -LiteralPath (Join-Path $mediaBin 'ffmpeg.exe'))) { throw 'Missing project-local FFmpeg.' }
$env:PATH = "$mediaBin;$env:PATH"
$env:PYTHONUTF8 = '1'
$env:PYTHONUNBUFFERED = '1'
$env:FFMPEG_PATH = Join-Path $mediaBin 'ffmpeg.exe'
$env:FFPROBE_PATH = Join-Path $mediaBin 'ffprobe.exe'
$env:DEVICE = 'cuda'
$env:DEMUCS_DEVICE = 'cuda'
$env:WHISPER_DEVICE = 'cuda'
$env:VOXCPM_DEVICE = 'cuda'
$env:VOXCPM_OPTIMIZE = 'false'
$env:NEXT_SERVER_API_BASE_URL = 'http://127.0.0.1:8000'
$env:NEXT_SERVER_PUBLICATION_API_BASE_URL = 'http://127.0.0.1:8000'
$env:YOUDUB_API_ONLY = 'false'

& $pythonExe -c "import torch; assert torch.cuda.is_available(), 'CUDA unavailable'; x=torch.ones(8,8,device='cuda'); y=x@x; torch.cuda.synchronize(); print('GPU: '+torch.cuda.get_device_name(0))"
if ($LASTEXITCODE -ne 0) { throw 'GPU check failed. No CPU fallback was started.' }
foreach ($port in @(8000, 3000)) {
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $port)
    try { $listener.Start() } catch { throw "Port $port is already in use. Close the previous YouDub instance first." } finally { $listener.Stop() }
}
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$backendProcess = $null
Push-Location $projectRoot
try {
    $backendProcess = Start-Process -FilePath $pythonExe -ArgumentList @('-m', 'uvicorn', 'backend.app.main:app', '--host', '127.0.0.1', '--port', '8000') -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logDir 'backend.stdout.log') -RedirectStandardError (Join-Path $logDir 'backend.stderr.log')
    $healthy = $false
    for ($attempt = 0; $attempt -lt 60; $attempt++) {
        if ($backendProcess.HasExited) { throw "Backend exited. See $logDir\backend.stderr.log" }
        try {
            $response = Invoke-WebRequest -Uri 'http://127.0.0.1:8000/api/health' -UseBasicParsing -TimeoutSec 2
            if ($response.StatusCode -eq 200) { $healthy = $true; break }
        } catch { Start-Sleep -Seconds 1 }
    }
    if (!$healthy) { throw "Backend startup timed out. See $logDir\backend.stderr.log" }
    Write-Host 'YouDub: http://127.0.0.1:3000'
    Write-Host 'Keep this window open. Press Ctrl+C to stop.'
    Set-Location (Join-Path $projectRoot 'apps\web')
    & node $nextCli dev --hostname 127.0.0.1 --port 3000
    if ($LASTEXITCODE -ne 0) { throw "Frontend exited with code $LASTEXITCODE" }
} finally {
    if ($backendProcess -and !$backendProcess.HasExited) {
        # A Windows venv launcher spawns the real Python interpreter as a child.
        & taskkill.exe /PID $backendProcess.Id /T /F 2>$null | Out-Null
    }
    Pop-Location
}
