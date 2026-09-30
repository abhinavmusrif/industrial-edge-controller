// Industrial Edge Controller Live Telemetry Client

async function fetchTelemetry() {
    try {
        const response = await fetch('/api/telemetry');
        if (!response.ok) return;
        const data = await response.json();
        updateDashboard(data);
    } catch (err) {
        console.error("Telemetry fetch error:", err);
    }
}

function updateDashboard(data) {
    document.getElementById('val-device').textContent = data.device || 'device01';
    
    // Values
    const temp = data.temperature || 0;
    const curr = data.current || 0;
    const vibe = data.vibration || 0;
    const rpm  = data.rpm || 0;

    document.getElementById('val-temp').textContent = temp.toFixed(1);
    document.getElementById('val-current').textContent = curr.toFixed(2);
    document.getElementById('val-vibe').textContent = vibe.toFixed(2);
    document.getElementById('val-rpm').textContent = Math.round(rpm);

    // Progress Bars
    updateProgressBar('bar-temp', temp, 20, 100, 70, 85);
    updateProgressBar('bar-current', curr, 0, 20, 10, 15);
    updateProgressBar('bar-vibe', vibe, 0, 15, 4, 7);
    updateProgressBar('bar-rpm', rpm, 0, 3600, 2800, 3200);

    // Badges & Diagnostics
    const mcuBadge = document.getElementById('badge-mcu');
    if (data.mcu_connected) {
        mcuBadge.textContent = 'MCU: CONNECTED';
        mcuBadge.className = 'pill status-ok';
    } else {
        mcuBadge.textContent = 'MCU: DISCONNECTED';
        mcuBadge.className = 'pill status-bad';
    }

    const wdBadge = document.getElementById('badge-watchdog');
    if (data.watchdog === 'OK') {
        wdBadge.textContent = 'WATCHDOG: OK';
        wdBadge.className = 'pill status-ok';
    } else {
        wdBadge.textContent = 'WATCHDOG: EXPIRED';
        wdBadge.className = 'pill status-bad';
    }

    const stateBadge = document.getElementById('badge-state');
    const stateStr = data.state || 'OFF';
    stateBadge.textContent = stateStr;
    if (stateStr === 'RUNNING') stateBadge.className = 'pill pill-large status-running';
    else if (stateStr === 'WARNING') stateBadge.className = 'pill pill-large status-warning';
    else if (stateStr === 'EMERGENCY_STOP' || stateStr === 'SAFE_STOP') stateBadge.className = 'pill pill-large status-estop';
    else stateBadge.className = 'pill pill-large';

    document.getElementById('diag-state').textContent = stateStr;
    document.getElementById('diag-trip').textContent = data.last_trip_reason || 'None';
    document.getElementById('diag-wd-time').textContent = (data.watchdog_elapsed_ms || 0) + ' ms';
    document.getElementById('diag-timestamp').textContent = (data.timestamp || '').split('T')[1] || data.timestamp;

    // Fault List
    const faultContainer = document.getElementById('fault-list');
    faultContainer.innerHTML = '';
    const faults = data.faults || [];
    if (faults.length === 0) {
        faultContainer.innerHTML = '<div class="fault-item fault-none">No active faults. All parameters nominal.</div>';
    } else {
        faults.forEach(f => {
            const div = document.createElement('div');
            div.className = 'fault-item fault-crit';
            div.textContent = f;
            faultContainer.appendChild(div);
        });
    }
}

function updateProgressBar(barId, val, min, max, warn, crit) {
    const bar = document.getElementById(barId);
    let pct = Math.max(0, Math.min(100, ((val - min) / (max - min)) * 100));
    bar.style.width = pct + '%';

    if (val >= crit) {
        bar.style.backgroundColor = 'var(--accent-red)';
    } else if (val >= warn) {
        bar.style.backgroundColor = 'var(--accent-yellow)';
    } else {
        bar.style.backgroundColor = 'var(--accent-blue)';
    }
}

async function sendActuatorCmd(cmd) {
    try {
        await fetch('/api/actuator', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: cmd })
        });
        setTimeout(fetchTelemetry, 150);
    } catch (e) {
        console.error("Actuator command failed:", e);
    }
}

async function injectFault(faultType) {
    try {
        await fetch('/api/fault', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ fault: faultType })
        });
        setTimeout(fetchTelemetry, 300);
    } catch (e) {
        console.error("Fault injection failed:", e);
    }
}

// Poll telemetry every 300ms
setInterval(fetchTelemetry, 300);
window.onload = fetchTelemetry;
