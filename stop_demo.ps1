# Industrial Edge Controller Demo Terminator (PowerShell)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

Write-Host "Stopping Industrial Edge Controller Simulation Demo..." -ForegroundColor Yellow

function Stop-PidFile($file, $name) {
    $path = Join-Path $ScriptDir $file
    if (Test-Path $path) {
        $procId = Get-Content $path -ErrorAction SilentlyContinue
        if ($procId) {
            try {
                $p = Get-Process -Id ([int]$procId) -ErrorAction SilentlyContinue
                if ($p) {
                    Write-Host "  Stopping $name (PID: $procId)..."
                    Stop-Process -Id ([int]$procId) -Force -ErrorAction SilentlyContinue
                }
            } catch {}
        }
        Remove-Item $path -Force -ErrorAction SilentlyContinue
    }
}

Stop-PidFile "dashboard.pid" "Web Dashboard"
Stop-PidFile "mcu.pid" "MCU Simulator"
Stop-PidFile "gateway.pid" "Linux Gateway"

# Fallback kill by name if orphaned
Get-Process edge_gateway -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Get-Process mcu_simulator -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

Write-Host "All digital twin simulation processes stopped cleanly." -ForegroundColor Green
