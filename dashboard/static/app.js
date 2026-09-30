// Industrial Edge Controller Live Telemetry, 3D WebGL Digital Twin & Linux Web Terminal

let scene, camera, renderer;
let motorGroup, shaftMesh, statorMesh, vibrationGroup, fanMesh;
let ledSys, ledRun, ledAlm, ledErr;
let isDragging = false;
let previousMousePosition = { x: 0, y: 0 };
let liveRpm = 0;
let liveTemp = 48.0;
let liveCurr = 6.5;
let liveVibe = 1.8;
let liveState = 'OFF';
let wireframeActive = false;
let autoRotateActive = false;
let cameraTarget = new THREE.Vector3(0, 0, 0);

// Command history for Linux Web Terminal
let cmdHistory = [];
let historyIndex = -1;

// --- 3D Digital Twin Engine (Three.js) ---
function init3DScene() {
    const container = document.getElementById('threejs-container');
    if (!container || typeof THREE === 'undefined') {
        console.warn("Three.js not loaded or container not found; fallback to CAD gallery");
        switchVisualView('cad');
        return;
    }

    const width = container.clientWidth || 800;
    const height = container.clientHeight || 420;

    scene = new THREE.Scene();
    scene.background = new THREE.Color(0x090d12);

    camera = new THREE.PerspectiveCamera(40, width / height, 0.1, 100);
    camera.position.set(4.8, 3.2, 5.6);
    camera.lookAt(cameraTarget);

    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setSize(width, height);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.shadowMap.enabled = true;
    container.appendChild(renderer.domElement);

    // Studio Lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.75);
    scene.add(ambientLight);

    const dirLight1 = new THREE.DirectionalLight(0x70aaff, 1.4);
    dirLight1.position.set(6, 12, 8);
    scene.add(dirLight1);

    const dirLight2 = new THREE.DirectionalLight(0xffb070, 0.7);
    dirLight2.position.set(-6, -2, -6);
    scene.add(dirLight2);

    // Floor Grid
    const gridHelper = new THREE.GridHelper(12, 24, 0x30363d, 0x161b22);
    gridHelper.position.y = -1.2;
    scene.add(gridHelper);

    // Assembly Groups
    motorGroup = new THREE.Group();
    vibrationGroup = new THREE.Group();
    vibrationGroup.add(motorGroup);
    scene.add(vibrationGroup);

    buildAdvanced3DMotor(motorGroup);
    build3DEdgeGateway(scene);

    // Mouse Orbit Controls
    container.addEventListener('mousedown', (e) => {
        if (e.button === 0) { // Left click
            isDragging = true;
            previousMousePosition = { x: e.clientX, y: e.clientY };
        }
    });

    window.addEventListener('mouseup', () => { isDragging = false; });

    container.addEventListener('mousemove', (e) => {
        if (!isDragging) return;
        const deltaX = e.clientX - previousMousePosition.x;
        const deltaY = e.clientY - previousMousePosition.y;

        vibrationGroup.rotation.y += deltaX * 0.008;
        vibrationGroup.rotation.x += deltaY * 0.008;
        previousMousePosition = { x: e.clientX, y: e.clientY };
    });

    container.addEventListener('wheel', (e) => {
        camera.position.z = Math.max(2.5, Math.min(12.0, camera.position.z + e.deltaY * 0.005));
    });

    window.addEventListener('resize', onWindowResize);
    animate();
}

function buildAdvanced3DMotor(parent) {
    // 1. Cast-Iron Ribbed Stator Main Cylinder
    const statorGeo = new THREE.CylinderGeometry(1.05, 1.05, 2.5, 36);
    const statorMat = new THREE.MeshStandardMaterial({
        color: 0x245580,
        roughness: 0.4,
        metalness: 0.75,
        emissive: 0x000000
    });
    statorMesh = new THREE.Mesh(statorGeo, statorMat);
    statorMesh.rotation.z = Math.PI / 2;
    parent.add(statorMesh);

    // 2. High-Density Cooling Fins Array (24 longitudinal ribs)
    const finMat = new THREE.MeshStandardMaterial({ color: 0x1c3d5a, roughness: 0.45, metalness: 0.8 });
    for (let i = 0; i < 24; i++) {
        const angle = (i / 24) * Math.PI * 2;
        const finGeo = new THREE.BoxGeometry(0.035, 2.3, 0.28);
        const fin = new THREE.Mesh(finGeo, finMat);
        fin.position.set(0, Math.cos(angle) * 1.1, Math.sin(angle) * 1.1);
        fin.rotation.x = angle;
        parent.add(fin);
    }

    // 3. Drive Shaft & Rotor
    const shaftGeo = new THREE.CylinderGeometry(0.24, 0.24, 4.0, 24);
    const shaftMat = new THREE.MeshStandardMaterial({ color: 0xe2e8f0, roughness: 0.15, metalness: 0.95 });
    shaftMesh = new THREE.Mesh(shaftGeo, shaftMat);
    shaftMesh.rotation.z = Math.PI / 2;
    parent.add(shaftMesh);

    // Shaft Keyway slot notch
    const keywayGeo = new THREE.BoxGeometry(0.35, 0.08, 0.08);
    const keywayMat = new THREE.MeshStandardMaterial({ color: 0x0f172a, roughness: 0.8 });
    const keyway = new THREE.Mesh(keywayGeo, keywayMat);
    keyway.position.set(1.7, 0.24, 0);
    shaftMesh.add(keyway);

    // Flexible Shaft Coupling (Couples motor to load)
    const coupGeo = new THREE.CylinderGeometry(0.42, 0.42, 0.5, 20);
    const coupMat = new THREE.MeshStandardMaterial({ color: 0x64748b, metalness: 0.85, roughness: 0.3 });
    const coupling = new THREE.Mesh(coupGeo, coupMat);
    coupling.rotation.z = Math.PI / 2;
    coupling.position.set(1.9, 0, 0);
    parent.add(coupling);

    // 4. Rear External Cooling Fan & Protective Shroud
    const fanCowlGeo = new THREE.CylinderGeometry(1.08, 1.08, 0.7, 32);
    const fanCowlMat = new THREE.MeshStandardMaterial({ color: 0x1e293b, metalness: 0.6, roughness: 0.4 });
    const fanCowl = new THREE.Mesh(fanCowlGeo, fanCowlMat);
    fanCowl.rotation.z = Math.PI / 2;
    fanCowl.position.set(-1.45, 0, 0);
    parent.add(fanCowl);

    // Rotating Fan Propeller
    fanMesh = new THREE.Group();
    const bladeMat = new THREE.MeshStandardMaterial({ color: 0x0284c7, roughness: 0.3 });
    for (let b = 0; b < 6; b++) {
        const bladeGeo = new THREE.BoxGeometry(0.12, 0.8, 0.04);
        const blade = new THREE.Mesh(bladeGeo, bladeMat);
        blade.rotation.z = (b / 6) * Math.PI * 2;
        fanMesh.add(blade);
    }
    fanMesh.position.set(-1.45, 0, 0);
    parent.add(fanMesh);

    // 5. Optical Shaft Quadrature Encoder (Enclosed on rear shaft extension)
    const encoderGeo = new THREE.CylinderGeometry(0.45, 0.45, 0.5, 24);
    const encoderMat = new THREE.MeshStandardMaterial({ color: 0x0f172a, roughness: 0.5, metalness: 0.7 });
    const encoder = new THREE.Mesh(encoderGeo, encoderMat);
    encoder.rotation.z = Math.PI / 2;
    encoder.position.set(-1.95, 0, 0);
    parent.add(encoder);

    // 6. Piezoelectric Vibration Accelerometer (Mounted rigidly on drive bearing)
    const accelGeo = new THREE.CylinderGeometry(0.12, 0.12, 0.38, 16);
    const accelMat = new THREE.MeshStandardMaterial({ color: 0xc084fc, metalness: 0.9, roughness: 0.1 });
    const accel = new THREE.Mesh(accelGeo, accelMat);
    accel.position.set(1.15, 1.05, 0);
    parent.add(accel);

    // 7. Terminal Junction Box on Stator Crown
    const termGeo = new THREE.BoxGeometry(0.75, 0.55, 0.75);
    const termMat = new THREE.MeshStandardMaterial({ color: 0x334155, roughness: 0.4, metalness: 0.6 });
    const termBox = new THREE.Mesh(termGeo, termMat);
    termBox.position.set(0, 1.35, 0);
    parent.add(termBox);

    // Brass cable gland nuts
    const glandGeo = new THREE.CylinderGeometry(0.08, 0.08, 0.2, 12);
    const glandMat = new THREE.MeshStandardMaterial({ color: 0xd97706, metalness: 0.8, roughness: 0.2 });
    const gland = new THREE.Mesh(glandGeo, glandMat);
    gland.position.set(0.38, 1.35, 0);
    gland.rotation.z = Math.PI / 2;
    parent.add(gland);

    // 8. Heavy-Duty Flanged Mounting Feet with Anchor Slots
    const footMat = new THREE.MeshStandardMaterial({ color: 0x1e2631, metalness: 0.75, roughness: 0.5 });
    const footGeo = new THREE.BoxGeometry(2.0, 0.18, 2.3);
    const foot = new THREE.Mesh(footGeo, footMat);
    foot.position.set(0, -1.08, 0);
    parent.add(foot);

    // Rigging Eye Bolt on top
    const eyeGeo = new THREE.TorusGeometry(0.14, 0.04, 12, 24);
    const eyeMat = new THREE.MeshStandardMaterial({ color: 0x94a3b8, metalness: 0.9 });
    const eyebolt = new THREE.Mesh(eyeGeo, eyeMat);
    eyebolt.position.set(0, 1.7, 0);
    parent.add(eyebolt);
}

function build3DEdgeGateway(parent) {
    const gwGroup = new THREE.Group();
    gwGroup.position.set(-3.2, -0.15, -0.4);
    gwGroup.rotation.y = 0.35;

    // DIN Rail Enclosure
    const boxGeo = new THREE.BoxGeometry(1.1, 2.0, 1.5);
    const boxMat = new THREE.MeshStandardMaterial({ color: 0x1e2631, roughness: 0.4, metalness: 0.8 });
    const box = new THREE.Mesh(boxGeo, boxMat);
    gwGroup.add(box);

    // Aluminum Heatsink fins
    const heatsinkMat = new THREE.MeshStandardMaterial({ color: 0x475569, metalness: 0.95 });
    for (let i = 0; i < 8; i++) {
        const fin = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.08, 1.4), heatsinkMat);
        fin.position.set(-0.57, -0.7 + i * 0.2, 0);
        gwGroup.add(fin);
    }

    // Status LEDs on front face
    const ledGeo = new THREE.SphereGeometry(0.045, 16, 16);
    ledSys = new THREE.Mesh(ledGeo, new THREE.MeshBasicMaterial({ color: 0x22c55e }));
    ledSys.position.set(0.56, 0.7, 0.45);
    gwGroup.add(ledSys);

    ledRun = new THREE.Mesh(ledGeo, new THREE.MeshBasicMaterial({ color: 0x388bfd }));
    ledRun.position.set(0.56, 0.5, 0.45);
    gwGroup.add(ledRun);

    ledAlm = new THREE.Mesh(ledGeo, new THREE.MeshBasicMaterial({ color: 0x334155 }));
    ledAlm.position.set(0.56, 0.3, 0.45);
    gwGroup.add(ledAlm);

    ledErr = new THREE.Mesh(ledGeo, new THREE.MeshBasicMaterial({ color: 0x334155 }));
    ledErr.position.set(0.56, 0.1, 0.45);
    gwGroup.add(ledErr);

    parent.add(gwGroup);
}

// Camera Preset Views
function setCameraView(preset) {
    document.querySelectorAll('.camera-toolbar .tool-btn').forEach(b => b.classList.remove('active'));
    event.target.classList.add('active');

    if (preset === 'overview') {
        camera.position.set(4.8, 3.2, 5.6);
        cameraTarget.set(0, 0, 0);
    } else if (preset === 'motor') {
        camera.position.set(1.5, 1.8, 3.2);
        cameraTarget.set(0, 0.2, 0);
    } else if (preset === 'encoder') {
        camera.position.set(-2.6, 0.8, 2.0);
        cameraTarget.set(-1.8, 0, 0);
    } else if (preset === 'controller') {
        camera.position.set(-3.2, 1.2, 1.8);
        cameraTarget.set(-3.2, -0.15, -0.4);
    }
    camera.lookAt(cameraTarget);
}

function toggleWireframe() {
    wireframeActive = !wireframeActive;
    event.target.classList.toggle('active', wireframeActive);
    motorGroup.traverse((child) => {
        if (child.isMesh && child.material) {
            child.material.wireframe = wireframeActive;
        }
    });
}

function toggleAutoRotate() {
    autoRotateActive = !autoRotateActive;
    event.target.classList.toggle('active', autoRotateActive);
}

function onWindowResize() {
    const container = document.getElementById('threejs-container');
    if (!container || !renderer || !camera) return;
    const width = container.clientWidth;
    const height = container.clientHeight;
    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    renderer.setSize(width, height);
}

// 3D Animation Loop
function animate() {
    requestAnimationFrame(animate);

    const deltaSec = 0.016;

    // 1. Rotor and Shaft Continuous Dynamic Rotation
    if (shaftMesh && liveRpm > 0 && liveState !== 'OFF') {
        const radPerSec = (liveRpm / 60) * Math.PI * 2;
        shaftMesh.rotation.x += radPerSec * deltaSec;
        if (fanMesh) fanMesh.rotation.x += radPerSec * deltaSec;
    }

    // 2. High-Frequency Accelerometer Shaking (Vibration RMS)
    if (vibrationGroup) {
        if (liveVibe > 0 && (liveState === 'RUNNING' || liveState === 'WARNING' || liveState === 'STARTING')) {
            const intensity = Math.min(0.04, (liveVibe / 10.0) * 0.035);
            vibrationGroup.position.x = (Math.random() - 0.5) * intensity;
            vibrationGroup.position.y = (Math.random() - 0.5) * intensity;
            vibrationGroup.position.z = (Math.random() - 0.5) * intensity;
        } else {
            vibrationGroup.position.set(0, 0, 0);
        }
    }

    // 3. Stator Dynamic Thermal Gradient
    if (statorMesh && statorMesh.material) {
        if (liveTemp > 85.0) {
            statorMesh.material.color.setHex(0xf85149); // Critical Red
            statorMesh.material.emissive.setHex(0x5a1010);
        } else if (liveTemp > 70.0) {
            statorMesh.material.color.setHex(0xd29922); // Warning Amber
            statorMesh.material.emissive.setHex(0x3a2500);
        } else {
            statorMesh.material.color.setHex(0x245580); // Nominal Blue
            statorMesh.material.emissive.setHex(0x000000);
        }
    }

    // 4. Edge Controller Status LEDs
    if (ledSys) {
        const pulse = (Math.sin(Date.now() * 0.005) + 1) * 0.5;
        ledSys.material.color.setRGB(0.1 + pulse * 0.9, 0.8, 0.2);
    }
    if (ledRun) {
        ledRun.material.color.setHex(liveState === 'RUNNING' ? 0x388bfd : 0x1e293b);
    }
    if (ledAlm) {
        ledAlm.material.color.setHex(liveState === 'WARNING' ? 0xd29922 : 0x1e293b);
    }
    if (ledErr) {
        ledErr.material.color.setHex((liveState === 'EMERGENCY_STOP' || liveState === 'SAFE_STOP') ? 0xf85149 : 0x1e293b);
    }

    // Turntable Auto-Rotate
    if (autoRotateActive && vibrationGroup) {
        vibrationGroup.rotation.y += 0.004;
    }

    renderer.render(scene, camera);
}

// Visual View Switcher (3D WebGL vs CAD Renders)
function switchVisualView(mode) {
    const container3d = document.getElementById('threejs-container');
    const containerCad = document.getElementById('cad-renders-container');
    const btn3d = document.getElementById('btn-view-3d');
    const btnCad = document.getElementById('btn-view-cad');

    if (mode === '3d') {
        container3d.style.display = 'block';
        containerCad.style.display = 'none';
        btn3d.className = 'btn btn-blue';
        btnCad.className = 'btn btn-gray';
        onWindowResize();
    } else {
        container3d.style.display = 'none';
        containerCad.style.display = 'block';
        btn3d.className = 'btn btn-gray';
        btnCad.className = 'btn btn-blue';
    }
}

// Terminal Shell & Hardware Registry Tabs
function switchTermTab(tab) {
    const viewCli = document.getElementById('term-view-cli');
    const viewHw  = document.getElementById('term-view-hw');
    const tabCli  = document.getElementById('tab-term-cli');
    const tabHw   = document.getElementById('tab-term-hw');

    if (tab === 'cli') {
        viewCli.style.display = 'block';
        viewHw.style.display = 'none';
        tabCli.classList.add('active');
        tabHw.classList.remove('active');
    } else {
        viewCli.style.display = 'none';
        viewHw.style.display = 'block';
        tabCli.classList.remove('active');
        tabHw.classList.add('active');
        fetchHardwareRegistry();
    }
}

// Embedded Linux Terminal Command Execution
async function executeTerminalCommand(cmd) {
    if (!cmd.trim()) return;

    cmdHistory.push(cmd);
    historyIndex = cmdHistory.length;

    const outElem = document.getElementById('terminal-output');
    outElem.textContent += `\nroot@industrial-edge:~# ${cmd}\n`;

    try {
        const res = await fetch('/api/terminal', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: cmd })
        });
        const data = await res.json();
        if (data.output) {
            outElem.textContent += data.output + "\n";
        }
        if (data.prompt) {
            document.getElementById('terminal-prompt').textContent = data.prompt;
        }
    } catch (e) {
        outElem.textContent += `[SHELL ERROR]: ${e}\n`;
    }

    const screen = document.getElementById('terminal-screen');
    screen.scrollTop = screen.scrollHeight;
}

function runQuickCmd(cmd) {
    const input = document.getElementById('terminal-input');
    input.value = cmd;
    executeTerminalCommand(cmd);
    input.value = '';
    input.focus();
}

// Fetch & Render Hardware IC Register Tree
async function fetchHardwareRegistry() {
    const container = document.getElementById('hw-registry-container');
    try {
        const res = await fetch('/api/hardware');
        const data = await res.json();
        
        let html = '';
        data.devices.forEach(dev => {
            html += `
            <div class="hw-card">
                <div class="hw-card-header">
                    <span class="hw-card-title">${dev.name} (${dev.part})</span>
                    <span class="hw-addr-badge">${dev.bus} @ ${dev.address}</span>
                </div>
                <div class="hw-card-desc">${dev.desc}</div>
                <table class="reg-table">
                    <thead>
                        <tr><th>Addr</th><th>Register</th><th>Raw Hex</th><th>Scaled Value</th></tr>
                    </thead>
                    <tbody>`;
            dev.registers.forEach(reg => {
                html += `
                    <tr>
                        <td class="reg-addr">${reg.addr}</td>
                        <td class="reg-name">${reg.name}</td>
                        <td class="reg-val">${reg.val}</td>
                        <td class="reg-scaled">${reg.scaled}</td>
                    </tr>`;
            });
            html += `</tbody></table></div>`;
        });

        // Virtual GPIO Bank
        html += `
        <div class="hw-card">
            <div class="hw-card-header">
                <span class="hw-card-title">Virtual GPIO Bank A</span>
                <span class="hw-addr-badge">Safety Interlock Lines</span>
            </div>
            <div class="hw-card-desc">Hardware contactor relays and PWM enable lines</div>
            <table class="reg-table">
                <thead>
                    <tr><th>Pin</th><th>Signal</th><th>Dir</th><th>State</th></tr>
                </thead>
                <tbody>`;
        data.gpio.forEach(g => {
            const stateLabel = g.val === 1 ? '<span style="color:#3fb950;font-weight:bold;">HIGH (1)</span>' : '<span style="color:#8b949e;">LOW (0)</span>';
            html += `
                <tr>
                    <td class="reg-addr">PIN ${g.pin}</td>
                    <td class="reg-name">${g.name}</td>
                    <td>${g.dir}</td>
                    <td>${stateLabel}</td>
                </tr>`;
        });
        html += `</tbody></table></div>`;

        container.innerHTML = html;
    } catch (e) {
        container.innerHTML = `<div style="color:var(--accent-red)">Failed to load hardware registry: ${e}</div>`;
    }
}

// Telemetry Poller
async function fetchTelemetry() {
    try {
        const res = await fetch('/api/telemetry');
        const data = await res.json();
        updateUI(data);
    } catch (e) {
        console.error("Telemetry fetch error:", e);
    }
}

function updateUI(data) {
    document.getElementById('val-device').textContent = data.device || 'device01';
    
    const temp = data.temperature || 0;
    const curr = data.current || 0;
    const vibe = data.vibration || 0;
    const rpm  = data.rpm || 0;

    liveRpm = rpm;
    liveTemp = temp;
    liveCurr = curr;
    liveVibe = vibe;
    liveState = data.state || 'OFF';

    document.getElementById('val-temp').textContent = temp.toFixed(1);
    document.getElementById('val-current').textContent = curr.toFixed(2);
    document.getElementById('val-vibe').textContent = vibe.toFixed(2);
    document.getElementById('val-rpm').textContent = Math.round(rpm);

    // Update 3D HUD
    const hudRpm = document.getElementById('hud-rpm');
    const hudTemp = document.getElementById('hud-temp');
    const hudCurr = document.getElementById('hud-curr');
    const hudVibe = document.getElementById('hud-vibe');
    if (hudRpm) hudRpm.textContent = Math.round(rpm) + ' RPM';
    if (hudTemp) hudTemp.textContent = temp.toFixed(1) + ' °C';
    if (hudCurr) hudCurr.textContent = curr.toFixed(2) + ' A';
    if (hudVibe) hudVibe.textContent = vibe.toFixed(2) + ' mm/s';

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

// Terminal Input Keyboard Handlers
function setupTerminalInput() {
    const input = document.getElementById('terminal-input');
    if (!input) return;

    input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            const val = input.value;
            input.value = '';
            executeTerminalCommand(val);
        } else if (e.key === 'ArrowUp') {
            if (cmdHistory.length > 0 && historyIndex > 0) {
                historyIndex--;
                input.value = cmdHistory[historyIndex];
            }
            e.preventDefault();
        } else if (e.key === 'ArrowDown') {
            if (historyIndex < cmdHistory.length - 1) {
                historyIndex++;
                input.value = cmdHistory[historyIndex];
            } else {
                historyIndex = cmdHistory.length;
                input.value = '';
            }
            e.preventDefault();
        }
    });
}

// Initial mount
window.onload = () => {
    init3DScene();
    setupTerminalInput();
    fetchTelemetry();
    setInterval(fetchTelemetry, 250);
};
