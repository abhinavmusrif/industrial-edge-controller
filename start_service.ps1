# Industrial Edge Controller Service Host
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $ScriptDir

$GwBin = Join-Path $ScriptDir "build\edge_gateway.exe"
$McuBin = Join-Path $ScriptDir "build\mcu_simulator.exe"

Write-Host "======================================================="
Write-Host " LAUNCHING INDUSTRIAL DIGITAL TWIN ON LOCALHOST"
Write-Host "======================================================="

# Ensure clean state
Stop-Process -Name edge_gateway, mcu_simulator -Force -ErrorAction SilentlyContinue

$gw = Start-Process -FilePath $GwBin -ArgumentList "--config config/gateway.conf" -RedirectStandardOutput "$ScriptDir\logs\gateway_stdout.log" -RedirectStandardError "$ScriptDir\logs\gateway_stderr.log" -PassThru
Write-Host "[OK] Gateway running (PID: $($gw.Id))"
Start-Sleep -Milliseconds 1200

$mcu = Start-Process -FilePath $McuBin -RedirectStandardOutput "$ScriptDir\logs\mcu_stdout.log" -RedirectStandardError "$ScriptDir\logs\mcu_stderr.log" -PassThru
Write-Host "[OK] MCU Simulator running (PID: $($mcu.Id))"
Start-Sleep -Milliseconds 800

$dash = Start-Process -FilePath "python" -ArgumentList "dashboard/app.py --port 8080" -RedirectStandardOutput "$ScriptDir\logs\dashboard.log" -RedirectStandardError "$ScriptDir\logs\dashboard_stderr.log" -PassThru
Write-Host "[OK] Web Dashboard running on http://localhost:8080 (PID: $($dash.Id))"

Write-Host "`nAll services active and listening on localhost!"
Write-Host "  - Web Dashboard:   http://localhost:8080"
Write-Host "  - CLI Monitor:      localhost:9100"
Write-Host "  - Modbus TCP:       localhost:1502"
Write-Host "  - MCU Link:         localhost:9000"
Write-Host "  - Fault Injector:   localhost:9001"

try {
    while ($true) {
        if ($gw.HasExited -or $mcu.HasExited -or $dash.HasExited) {
            Write-Host "[WARN] A component exited unexpectedly."
            break
        }
        Start-Sleep -Seconds 2
    }
} finally {
    Stop-Process -Id $gw.Id, $mcu.Id, $dash.Id -Force -ErrorAction SilentlyContinue
}
