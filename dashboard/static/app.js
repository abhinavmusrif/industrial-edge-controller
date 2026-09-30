// Industrial Edge Controller Live Telemetry & 3D WebGL Digital Twin

let scene, camera, renderer;
let motorGroup, shaftMesh, statorMesh, vibrationGroup;
let ledSys, ledRun, ledAlm, ledErr;
let isDragging = false;
let previousMousePosition = { x: 0, y: 0 };
let liveRpm = 0;
let liveTemp = 48.0;
let liveVibe = 1.8;
let liveState = 'OFF';

// --- 3D Digital Twin Engine (Three.js) ---
function init3DScene() {
    const container = document.getElementById('threejs-container');
    if (!container || typeof THREE === 'undefined') {
        console.warn("Three.js not loaded or container not found; fallback to CAD gallery");
        switchVisualView('cad');
        return;
    }

    const width = container.clientWidth || 800;
    const height = container.clientHeight || 380;

    scene = new THREE.Scene();
    scene.background = new THREE.Color(0x0e1217);

    camera = new THREE.PerspectiveCamera(40, width / height, 0.1, 100);
    camera.position.set(4.5, 3.2, 5.5);
    camera.lookAt(0, 0, 0);

    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setSize(width, height);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.shadowMap.enabled = true;
    container.appendChild(renderer.domElement);

    // Lights
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.7);
    scene.add(ambientLight);

    const dirLight1 = new THREE.DirectionalLight(0x70aaff, 1.2);
    dirLight1.position.set(5, 10, 7);
    scene.add(dirLight1);

    const dirLight2 = new THREE.DirectionalLight(0xffb070, 0.6);
    dirLight2.position.set(-5, -2, -5);
    scene.add(dirLight2);

    // Grid Floor
    const gridHelper = new THREE.GridHelper(10, 20, 0x334155, 0x1e293b);
    gridHelper.position.y = -1.2;
    scene.add(gridHelper);

    // Assembly Groups
    motorGroup = new THREE.Group();
    vibrationGroup = new THREE.Group();
    vibrationGroup.add(motorGroup);
    scene.add(vibrationGroup);

    build3DMotor(motorGroup);
    build3DEdgeGateway(scene);

    // Simple Mouse Orbit Drag
    container.addEventListener('mousedown', (e) => {
        isDragging = true;
        previousMousePosition = { x: e.clientX, y: e.clientY };
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
        camera.position.z = Math.max(3.0, Math.min(10.0, camera.position.z + e.deltaY * 0.005));
    });

    window.addEventListener('resize', onWindowResize);
    animate();
}

function build3DMotor(parent) {
    // 1. Stator Main Cylinder
    const statorGeo = new THREE.CylinderGeometry(1.0, 1.0, 2.4, 32);
    const statorMat = new THREE.MeshStandardMaterial({
        color: 0x2b5c8f,
        roughness: 0.35,
        metalness: 0.8
    });
    statorMesh = new THREE.Mesh(statorGeo, statorMat);
    statorMesh.rotation.z = Math.PI / 2;
    parent.add(statorMesh);

    // 2. Cooling Fins (Radially positioned longitudinal ribs)
    const finMat = new THREE.MeshStandardMaterial({ color: 0x1f4469, roughness: 0.4, metalness: 0.85 });
    for (let i = 0; i < 16; i++) {
        const angle = (i / 16) * Math.PI * 2;
        const finGeo = new THREE.BoxGeometry(0.04, 2.2, 0.25);
        const fin = new THREE.Mesh(finGeo, finMat);
        fin.position.set(0, Math.cos(angle) * 1.05, Math.sin(angle) * 1.05);
        fin.rotation.x = angle;
        parent.add(fin);
    }

    // 3. Drive Shaft & Rotor
    const shaftGeo = new THREE.CylinderGeometry(0.22, 0.22, 3.6, 24);
    const shaftMat = new THREE.MeshStandardMaterial({ color: 0xd1d5db, roughness: 0.2, metalness: 0.95 });
    shaftMesh = new THREE.Mesh(shaftGeo, shaftMat);
    shaftMesh.rotation.z = Math.PI / 2;
    parent.add(shaftMesh);

    // Shaft Keyway notch indicator (to clearly see rotation)
    const keywayGeo = new THREE.BoxGeometry(0.3, 0.08, 0.08);
    const keywayMat = new THREE.MeshStandardMaterial({ color: 0x111827 });
    const keyway = new THREE.Mesh(keywayGeo, keywayMat);
    keyway.position.set(1.6, 0.22, 0);
    shaftMesh.add(keyway);

    // 4. Optical Shaft Quadrature Encoder (Rear housing)
    const encoderGeo = new THREE.CylinderGeometry(0.5, 0.5, 0.6, 24);
    const encoderMat = new THREE.MeshStandardMaterial({ color: 0x111827, roughness: 0.4, metalness: 0.6 });
    const encoder = new THREE.Mesh(encoderGeo, encoderMat);
    encoder.rotation.z = Math.PI / 2;
    encoder.position.set(-1.45, 0, 0);
    parent.add(encoder);

    // 5. Vibration Sensor (Piezoelectric Accelerometer bolted to front bearing)
    const accelGeo = new THREE.CylinderGeometry(0.12, 0.12, 0.35, 16);
    const accelMat = new THREE.MeshStandardMaterial({ color: 0x94a3b8, metalness: 0.9, roughness: 0.2 });
    const accel = new THREE.Mesh(accelGeo, accelMat);
    accel.position.set(1.1, 0.95, 0);
    parent.add(accel);

    // 6. Terminal Box on top
    const termGeo = new THREE.BoxGeometry(0.7, 0.5, 0.7);
    const termMat = new THREE.MeshStandardMaterial({ color: 0x334155, roughness: 0.5 });
    const termBox = new THREE.Mesh(termGeo, termMat);
    termBox.position.set(0, 1.25, 0);
    parent.add(termBox);

    // 7. Mounting Feet Base
    const footGeo = new THREE.BoxGeometry(1.8, 0.15, 2.2);
    const footMat = new THREE.MeshStandardMaterial({ color: 0x1e293b, metalness: 0.7 });
    const foot = new THREE.Mesh(footGeo, footMat);
    foot.position.set(0, -1.05, 0);
    parent.add(foot);
}

function build3DEdgeGateway(parent) {
    const gwGroup = new THREE.Group();
    gwGroup.position.set(-2.8, -0.2, -0.5);
    gwGroup.rotation.y = 0.3;

    // DIN Rail enclosure
    const boxGeo = new THREE.BoxGeometry(1.0, 1.8, 1.4);
    const boxMat = new THREE.MeshStandardMaterial({ color: 0x1e2631, roughness: 0.4, metalness: 0.8 });
    const box = new THREE.Mesh(boxGeo, boxMat);
    gwGroup.add(box);

    // Aluminum Heatsink fins on side
    const heatsinkMat = new THREE.MeshStandardMaterial({ color: 0x374151, metalness: 0.9 });
    for (let i = 0; i < 7; i++) {
        const fin = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.08, 1.3), heatsinkMat);
        fin.position.set(-0.52, -0.6 + i * 0.2, 0);
        gwGroup.add(fin);
    }

    // Status LEDs on front face
    const ledGeo = new THREE.SphereGeometry(0.04, 16, 16);
    ledSys = new THREE.Mesh(ledGeo, new THREE.MeshBasicMaterial({ color: 0x22c55e }));
    ledSys.position.set(0.51, 0.6, 0.4);
    gwGroup.add(ledSys);

    ledRun = new THREE.Mesh(ledGeo, new THREE.MeshBasicMaterial({ color: 0x22c55e }));
    ledRun.position.set(0.51, 0.45, 0.4);
    gwGroup.add(ledRun);

    ledAlm = new THREE.Mesh(ledGeo, new THREE.MeshBasicMaterial({ color: 0x4b5563 }));
    ledAlm.position.set(0.51, 0.3, 0.4);
    gwGroup.add(ledAlm);

    ledErr = new THREE.Mesh(ledGeo, new THREE.MeshBasicMaterial({ color: 0x4b5563 }));
    ledErr.position.set(0.51, 0.15, 0.4);
    gwGroup.add(ledErr);

    parent.add(gwGroup);
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

function animate() {
    requestAnimationFrame(animate);

    // 1. Rotate Shaft proportional to RPM
    if (shaftMesh && liveRpm > 0) {
        shaftMesh.rotation.x += (liveRpm / 60) * 0.08;
    }

    // 2. Color Heat-Mapping on Stator based on Temperature
    if (statorMesh) {
        if (liveTemp >= 85.0) {
            // Critical Over-Temperature: Pulsing Red
            const pulse = (Math.sin(Date.now() * 0.01) + 1) * 0.5;
            statorMesh.material.color.setRGB(0.9 + pulse * 0.1, 0.1, 0.1);
        } else if (liveTemp >= 70.0) {
            // Warning Band: Amber
            statorMesh.material.color.setHex(0xd97706);
        } else {
            // Nominal Temperature (Color blends from cool blue to warm green)
            const tPct = Math.max(0, Math.min(1, (liveTemp - 35) / 35));
            statorMesh.material.color.setRGB(0.18 + tPct * 0.1, 0.36 + tPct * 0.2, 0.56 - tPct * 0.2);
        }
    }

    // 3. Vibration Displacement Shake
    if (vibrationGroup) {
        if (liveVibe > 0.5) {
            const shake = (liveVibe / 10.0) * 0.035;
            vibrationGroup.position.x = (Math.random() - 0.5) * shake;
            vibrationGroup.position.y = (Math.random() - 0.5) * shake;
            vibrationGroup.position.z = (Math.random() - 0.5) * shake;
        } else {
            vibrationGroup.position.set(0, 0, 0);
        }
    }

    // 4. Edge Gateway LEDs
    if (ledRun && ledAlm && ledErr) {
        if (liveState === 'RUNNING') {
            ledRun.material.color.setHex(0x22c55e);
            ledAlm.material.color.setHex(0x374151);
            ledErr.material.color.setHex(0x374151);
        } else if (liveState === 'WARNING') {
            ledRun.material.color.setHex(0x22c55e);
            ledAlm.material.color.setHex(0xf59e0b);
            ledErr.material.color.setHex(0x374151);
        } else if (liveState === 'EMERGENCY_STOP' || liveState === 'SAFE_STOP') {
            ledRun.material.color.setHex(0x374151);
            ledAlm.material.color.setHex(0x374151);
            ledErr.material.color.setHex(0xef4444);
        } else {
            ledRun.material.color.setHex(0x374151);
            ledAlm.material.color.setHex(0x374151);
            ledErr.material.color.setHex(0x374151);
        }
    }

    renderer.render(scene, camera);
}

function switchVisualView(mode) {
    const threeContainer = document.getElementById('threejs-container');
    const cadContainer = document.getElementById('cad-renders-container');
    const btn3d = document.getElementById('btn-view-3d');
    const btnCad = document.getElementById('btn-view-cad');

    if (mode === '3d') {
        threeContainer.style.display = 'block';
        cadContainer.style.display = 'none';
        btn3d.className = 'btn btn-blue';
        btnCad.className = 'btn btn-gray';
        onWindowResize();
    } else {
        threeContainer.style.display = 'none';
        cadContainer.style.display = 'block';
        btn3d.className = 'btn btn-gray';
        btnCad.className = 'btn btn-blue';
    }
}

// --- Live Telemetry API ---
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

    liveRpm = rpm;
    liveTemp = temp;
    liveVibe = vibe;
    liveState = data.state || 'OFF';

    document.getElementById('val-temp').textContent = temp.toFixed(1);
    document.getElementById('val-current').textContent = curr.toFixed(2);
    document.getElementById('val-vibe').textContent = vibe.toFixed(2);
    document.getElementById('val-rpm').textContent = Math.round(rpm);

    // Update 3D HUD
    const hudRpm = document.getElementById('hud-rpm');
    const hudTemp = document.getElementById('hud-temp');
    const hudVibe = document.getElementById('hud-vibe');
    if (hudRpm) hudRpm.textContent = Math.round(rpm) + ' RPM';
    if (hudTemp) hudTemp.textContent = temp.toFixed(1) + ' °C';
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

// Initial mount
window.onload = () => {
    init3DScene();
    fetchTelemetry();
    setInterval(fetchTelemetry, 250);
};
