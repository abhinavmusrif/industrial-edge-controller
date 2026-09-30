# Industrial Edge Controller Demo Runner (PowerShell)
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $ScriptDir

Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host " Starting Industrial Edge Controller Simulation Demo" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan

# Locate binaries
$GwBin = Join-Path $ScriptDir "build\edge_gateway.exe"
if (-not (Test-Path $GwBin)) { $GwBin = Join-Path $ScriptDir "build\bin\edge_gateway.exe" }
if (-not (Test-Path $GwBin)) { $GwBin = Join-Path $ScriptDir "build\Release\edge_gateway.exe" }

$McuBin = Join-Path $ScriptDir "build\mcu_simulator.exe"
if (-not (Test-Path $McuBin)) { $McuBin = Join-Path $ScriptDir "build\bin\mcu_simulator.exe" }
if (-not (Test-Path $McuBin)) { $McuBin = Join-Path $ScriptDir "build\Release\mcu_simulator.exe" }

if (-not (Test-Path $GwBin)) {
    Write-Host "[INFO] Binaries not found. Running build.bat first..." -ForegroundColor Yellow
    & "$ScriptDir\build.bat"
}

if (-not (Test-Path "$ScriptDir\logs")) { New-Item -ItemType Directory -Path "$ScriptDir\logs" -Force | Out-Null }

# Stop any running instances
& "$ScriptDir\stop_demo.ps1"
Start-Sleep -Milliseconds 800

# 1. Start Gateway
Write-Host "[INFO] Starting Linux Edge Gateway..." -ForegroundColor Green
$gwProc = Start-Process -FilePath $GwBin -ArgumentList "--config config/gateway.conf" -RedirectStandardOutput "$ScriptDir\logs\gateway_stdout.log" -RedirectStandardError "$ScriptDir\logs\gateway_stderr.log" -PassThru
$gwProc.Id | Set-Content "$ScriptDir\gateway.pid"
Start-Sleep -Milliseconds 1200

# 2. Start MCU Simulator
Write-Host "[INFO] Starting MCU Simulator..." -ForegroundColor Green
$mcuProc = Start-Process -FilePath $McuBin -RedirectStandardOutput "$ScriptDir\logs\mcu_stdout.log" -RedirectStandardError "$ScriptDir\logs\mcu_stderr.log" -PassThru
$mcuProc.Id | Set-Content "$ScriptDir\mcu.pid"
Start-Sleep -Milliseconds 1000

# 3. Start Web Dashboard
Write-Host "[INFO] Starting Web Dashboard on http://localhost:8080..." -ForegroundColor Green
$dashProc = Start-Process -FilePath "python" -ArgumentList "dashboard/app.py --port 8080" -RedirectStandardOutput "$ScriptDir\logs\dashboard.log" -RedirectStandardError "$ScriptDir\logs\dashboard_stderr.log" -PassThru
$dashProc.Id | Set-Content "$ScriptDir\dashboard.pid"

Write-Host ""
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host " DIGITAL TWIN SIMULATION RUNNING" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "  🌐 Web Dashboard:    http://localhost:8080" -ForegroundColor White
Write-Host "  💻 CLI TCP Monitor:   python tools/tcp_monitor.py -i" -ForegroundColor White
Write-Host "  ⚡ Modbus TCP:       python tools/modbus_client.py" -ForegroundColor White
Write-Host "  🧰 Fault Injector:   python tools/fault_injector.py --temperature-high" -ForegroundColor White
Write-Host "  📋 System Logs:      logs/gateway.log" -ForegroundColor White
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "To stop demo: .\stop_demo.ps1 or stop_demo.bat" -ForegroundColor Yellow
