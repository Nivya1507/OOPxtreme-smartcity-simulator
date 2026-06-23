const canvas = document.getElementById('simCanvas');
const ctx = canvas.getContext('2d');

let isPlaying = true;
let simInterval = null;
let lastState = null;
let knownLogs = new Set();

// Setup UI Element bindings
const playBtn = document.getElementById('play-btn');
const stepBtn = document.getElementById('step-btn');
const spawnBtn = document.getElementById('spawn-btn');
const clearLogsBtn = document.getElementById('clear-logs-btn');
const logFeed = document.getElementById('log-feed');
const telemetryBody = document.querySelector('#telemetry-table tbody');
const vehicleIdInput = document.getElementById('vehicle-id');
const vehicleTypeSelect = document.getElementById('vehicle-type');
const vehicleRouteSelect = document.getElementById('vehicle-route');
const simStatusTxt = document.getElementById('sim-status-txt');

// Coordinates helper (scaling 800x800 canvas)
function getIntersectionCoords(nodeId) {
    const nodes = {
        'A': { x: 150, y: 150 },
        'B': { x: 400, y: 150 },
        'C': { x: 650, y: 150 },
        'D': { x: 150, y: 400 },
        'E': { x: 400, y: 400 },
        'F': { x: 650, y: 400 },
        'G': { x: 150, y: 650 },
        'H': { x: 400, y: 650 },
        'I': { x: 650, y: 650 }
    };
    return nodes[nodeId] || { x: 0, y: 0 };
}

// Distance from point to line segment formula for road selection
function getDistanceToSegment(px, py, x1, y1, x2, y2) {
    const dx = x2 - x1;
    const dy = y2 - y1;
    const lenSq = dx * dx + dy * dy;
    if (lenSq === 0) return Math.hypot(px - x1, py - y1);
    
    let t = ((px - x1) * dx + (py - y1) * dy) / lenSq;
    t = Math.max(0, Math.min(1, t)); // clamp to line segment
    
    const projX = x1 + t * dx;
    const projY = y1 + t * dy;
    return Math.hypot(px - projX, py - projY);
}

// Draw the simulation
function draw() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    if (!lastState) return;

    // 1. Draw Grid Roads
    lastState.roads.forEach(road => {
        const p1 = getIntersectionCoords(road.from);
        const p2 = getIntersectionCoords(road.to);

        ctx.beginPath();
        ctx.moveTo(p1.x, p1.y);
        ctx.lineTo(p2.x, p2.y);

        if (road.blocked) {
            ctx.strokeStyle = '#ff0055';
            ctx.lineWidth = 6;
            ctx.setLineDash([8, 8]);
        } else {
            ctx.strokeStyle = 'rgba(0, 242, 254, 0.2)';
            ctx.lineWidth = 10;
            ctx.setLineDash([]);
            ctx.stroke();

            // Inner lane path glow
            ctx.beginPath();
            ctx.moveTo(p1.x, p1.y);
            ctx.lineTo(p2.x, p2.y);
            ctx.strokeStyle = '#1e293b';
            ctx.lineWidth = 4;
        }
        ctx.stroke();
        ctx.setLineDash([]); // Reset

        // If road has vehicles close to capacity, draw congestion overlay
        if (road.vehicles > 0 && !road.blocked) {
            const densityRatio = road.vehicles / road.capacity;
            if (densityRatio >= 0.8) {
                ctx.beginPath();
                ctx.moveTo(p1.x, p1.y);
                ctx.lineTo(p2.x, p2.y);
                ctx.strokeStyle = 'rgba(255, 179, 0, 0.4)';
                ctx.lineWidth = 8;
                ctx.stroke();
            }
        }
    });

    // 2. Draw Intersections
    lastState.intersections.forEach(inter => {
        const coords = getIntersectionCoords(inter.id);
        
        // Draw outer glow circle
        ctx.beginPath();
        ctx.arc(coords.x, coords.y, 24, 0, 2 * Math.PI);
        ctx.fillStyle = 'rgba(16, 22, 38, 0.85)';
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
        ctx.lineWidth = 2;
        ctx.fill();
        ctx.stroke();

        // Draw inner core circle
        ctx.beginPath();
        ctx.arc(coords.x, coords.y, 8, 0, 2 * Math.PI);
        ctx.fillStyle = '#00f2fe';
        ctx.shadowColor = '#00f2fe';
        ctx.shadowBlur = 10;
        ctx.fill();
        ctx.shadowBlur = 0; // Reset glow

        // Text label
        ctx.font = '600 12px Outfit';
        ctx.fillStyle = '#9ca3af';
        ctx.textAlign = 'center';
        ctx.fillText(inter.name, coords.x, coords.y - 32);
        
        ctx.font = '800 13px Space Grotesk';
        ctx.fillStyle = '#080c14';
        ctx.fillText(inter.id, coords.x, coords.y + 4);
    });

    // 3. Draw Traffic Signals
    lastState.signals.forEach(sig => {
        const coords = getIntersectionCoords(sig.intersectionId);
        
        // Offset the signal light slightly from the intersection center
        const offset = 18; 
        let sx = coords.x + offset;
        let sy = coords.y - offset;

        ctx.beginPath();
        ctx.arc(sx, sy, 8, 0, 2 * Math.PI);
        
        let color = '#ff0055'; // Red default
        if (sig.light === 'Green') color = '#00f2a1';
        else if (sig.light === 'Yellow') color = '#ffb300';

        ctx.fillStyle = color;
        ctx.shadowColor = color;
        ctx.shadowBlur = 12;
        ctx.fill();
        ctx.shadowBlur = 0; // Reset
        
        // Draw tiny border
        ctx.strokeStyle = 'rgba(0,0,0,0.6)';
        ctx.lineWidth = 1;
        ctx.stroke();
    });

    // 4. Draw Vehicles
    lastState.vehicles.forEach(veh => {
        // Find interpolated positions
        const fromInter = getIntersectionCoords(veh.from);
        const toInter = getIntersectionCoords(veh.to);
        
        // Calculate coordinates based on progress
        const vx = fromInter.x + (toInter.x - fromInter.x) * veh.progress;
        const vy = fromInter.y + (toInter.y - fromInter.y) * veh.progress;

        let baseColor = '#7000ff'; // Bus purple
        let vehicleSize = 14;

        if (veh.type === 'AutonomousElectricBus') {
            baseColor = '#ff00d4'; // Electric pink
        } else if (veh.type === 'EmergencyVehicle') {
            baseColor = '#00f2fe'; // Emergency cyan
            
            // Pulsing emergency circle rings
            const pulseRadius = 16 + (Date.now() % 400) / 20;
            ctx.beginPath();
            ctx.arc(vx, vy, pulseRadius, 0, 2 * Math.PI);
            ctx.strokeStyle = (Date.now() % 200 > 100) ? 'rgba(255, 0, 85, 0.4)' : 'rgba(0, 242, 254, 0.4)';
            ctx.lineWidth = 2;
            ctx.stroke();
        }

        ctx.shadowColor = baseColor;
        ctx.shadowBlur = 8;
        
        // Draw Vehicle body (Square with arrow-direction marker)
        ctx.fillStyle = baseColor;
        ctx.fillRect(vx - vehicleSize/2, vy - vehicleSize/2, vehicleSize, vehicleSize);
        ctx.shadowBlur = 0; // Reset

        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 1.5;
        ctx.strokeRect(vx - vehicleSize/2, vy - vehicleSize/2, vehicleSize, vehicleSize);

        // Render ID text label above vehicle
        ctx.font = '700 10px Space Grotesk';
        ctx.fillStyle = '#ffffff';
        ctx.fillText(veh.id, vx, vy - 10);
    });
}

// Fetch current simulation state
async function fetchStatus() {
    try {
        const res = await fetch('/api/status');
        if (!res.ok) throw new Error("Status API error");
        
        const data = await res.json();
        lastState = data;
        
        updateTelemetryTable(data.vehicles);
        updateLogs(data.logs);
        draw();
    } catch (err) {
        console.error(err);
    }
}

// Step the C++ simulation engine via REST call
async function triggerStep(dtValue = 1.0) {
    try {
        await fetch(`/api/step?dt=${dtValue}`);
        await fetchStatus();
    } catch (err) {
        console.error("Step failed:", err);
    }
}

// Update telemetry UI list
function updateTelemetryTable(vehicles) {
    if (!vehicles || vehicles.length === 0) {
        telemetryBody.innerHTML = `
            <tr class="empty-row">
                <td colspan="7">No active vehicles. Spawn one to start.</td>
            </tr>
        `;
        return;
    }

    let html = '';
    vehicles.forEach(veh => {
        let typeBadge = '';
        let extraInfo = 'N/A';

        if (veh.type === 'Bus') {
            typeBadge = '<span class="table-badge bus">Bus</span>';
        } else if (veh.type === 'AutonomousElectricBus') {
            typeBadge = '<span class="table-badge electric">Electric Bus</span>';
            const batClass = veh.battery > 50 ? '' : (veh.battery > 20 ? 'warning' : 'danger');
            extraInfo = `
                <div class="battery-bar">
                    <div class="battery-fill ${batClass}" style="width: ${veh.battery}%"></div>
                </div> 
                <span style="font-size:0.75rem; font-family:var(--font-mono)">${veh.battery.toFixed(1)}%</span>
            `;
        } else if (veh.type === 'EmergencyVehicle') {
            typeBadge = '<span class="table-badge emergency">Emergency</span>';
            extraInfo = veh.siren === 'Siren_Active' ? '<span style="color:#ff0055; font-weight:bold; animation:pulse 0.5s infiniteAlternate">🚨 SIREN</span>' : 'Siren Off';
        }

        html += `
            <tr>
                <td style="font-family:var(--font-mono); font-weight:600">${veh.id}</td>
                <td>${typeBadge}</td>
                <td style="font-family:var(--font-mono)">${veh.from} &rarr; ${veh.to}</td>
                <td style="font-family:var(--font-mono)">${veh.speed.toFixed(1)} km/h</td>
                <td style="font-family:var(--font-mono)">${veh.passengers !== undefined ? `${veh.passengers}/${veh.capacity}` : '-'}</td>
                <td style="font-family:var(--font-mono); color:var(--success-color)">${veh.fares !== undefined ? `$${veh.fares.toFixed(2)}` : '-'}</td>
                <td>${extraInfo}</td>
            </tr>
        `;
    });
    telemetryBody.innerHTML = html;
}

// Append new engine exceptions/logs to scrolling text box
function updateLogs(logs) {
    if (!logs) return;
    
    logs.forEach(log => {
        if (!knownLogs.has(log)) {
            knownLogs.add(log);
            
            const logDiv = document.createElement('div');
            logDiv.className = 'log-entry';
            
            // Check content to apply class
            if (log.includes('Exception') || log.includes('blocked') || log.includes('depleted')) {
                logDiv.className += ' error';
            } else if (log.includes('Spawned') || log.includes('Loaded')) {
                logDiv.className += ' success';
            } else if (log.includes('Signal')) {
                logDiv.className += ' info';
            } else {
                logDiv.className += ' system';
            }
            
            logDiv.textContent = log;
            logFeed.appendChild(logDiv);
            
            // Keep logs scrolled down
            logFeed.scrollTop = logFeed.scrollHeight;
        }
    });
}

// Start simulation automation loop
function startSimulationLoop() {
    isPlaying = true;
    playBtn.innerHTML = `
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="6" y="4" width="4" height="16"></rect><rect x="14" y="4" width="4" height="16"></rect></svg>
        Pause Sim
    `;
    playBtn.className = 'btn btn-secondary';
    simStatusTxt.textContent = "Engine: Running";
    document.querySelector('.dot').classList.add('pulse');

    simInterval = setInterval(() => {
        triggerStep(1.5); // Call backend to step the simulator
    }, 250); // Updates 4 times a second
}

// Pause simulation automation loop
function stopSimulationLoop() {
    isPlaying = false;
    playBtn.innerHTML = `
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5 3 19 12 5 21 5 3"></polygon></svg>
        Resume Sim
    `;
    playBtn.className = 'btn btn-primary';
    simStatusTxt.textContent = "Engine: Paused";
    document.querySelector('.dot').classList.remove('pulse');

    if (simInterval) {
        clearInterval(simInterval);
        simInterval = null;
    }
}

// Bind manual interactions
playBtn.addEventListener('click', () => {
    if (isPlaying) stopSimulationLoop();
    else startSimulationLoop();
});

stepBtn.addEventListener('click', () => {
    stopSimulationLoop();
    triggerStep(1.5);
});

clearLogsBtn.addEventListener('click', () => {
    logFeed.innerHTML = '';
    knownLogs.clear();
});

// Spawn API integration
spawnBtn.addEventListener('click', async () => {
    const type = vehicleTypeSelect.value;
    const id = vehicleIdInput.value.trim();
    const route = vehicleRouteSelect.value;

    if (!id) {
        alert("Please enter a unique vehicle ID");
        return;
    }

    try {
        const res = await fetch(`/api/spawn?type=${type}&id=${encodeURIComponent(id)}&route=${route}`);
        const data = await res.json();
        
        if (data.status === 'error') {
            const errDiv = document.createElement('div');
            errDiv.className = 'log-entry error';
            errDiv.textContent = `Spawn Error: ${data.message}`;
            logFeed.appendChild(errDiv);
            logFeed.scrollTop = logFeed.scrollHeight;
        } else {
            // Auto increment ID suffix for next spawn
            const num = parseInt(id.replace(/\D/g, '')) || 0;
            const prefix = id.replace(/[0-9]/g, '') || "BUS-";
            vehicleIdInput.value = `${prefix}${String(num + 1).padStart(2, '0')}`;
            
            fetchStatus();
        }
    } catch (err) {
        console.error("Spawn API call failed", err);
    }
});

// Canvas Click Handlers for Interactive Road Blocks and Signal Overrides
canvas.addEventListener('click', async (e) => {
    const rect = canvas.getBoundingClientRect();
    const clickX = ((e.clientX - rect.left) / rect.width) * canvas.width;
    const clickY = ((e.clientY - rect.top) / rect.height) * canvas.height;

    // 1. Check if clicked an intersection node (radius of 25px)
    if (lastState) {
        for (let inter of lastState.intersections) {
            const coords = getIntersectionCoords(inter.id);
            const dist = Math.hypot(clickX - coords.x, clickY - coords.y);
            if (dist < 25) {
                // Find current light state
                let currentLight = 'Green';
                const sig = lastState.signals.find(s => s.intersectionId === inter.id);
                if (sig) currentLight = sig.light;

                // Cycle light state: Green -> Red -> Yellow -> Green
                let nextLight = 'Green';
                if (currentLight === 'Green') nextLight = 'Red';
                else if (currentLight === 'Red') nextLight = 'Yellow';

                const sigId = `Signal_${inter.id}`;
                await fetch(`/api/override-signal?id=${sigId}&state=${nextLight}`);
                await fetchStatus();
                return;
            }
        }
        
        // 2. Check if clicked a road edge (within 15px distance to line segment)
        for (let road of lastState.roads) {
            const p1 = getIntersectionCoords(road.from);
            const p2 = getIntersectionCoords(road.to);
            const distToSegment = getDistanceToSegment(clickX, clickY, p1.x, p1.y, p2.x, p2.y);
            
            if (distToSegment < 15) {
                const nextBlocked = road.blocked ? 0 : 1;
                // Toggle road segment AND reverse segment block
                await fetch(`/api/block-road?from=${road.from}&to=${road.to}&blocked=${nextBlocked}`);
                await fetch(`/api/block-road?from=${road.to}&to=${road.from}&blocked=${nextBlocked}`);
                await fetchStatus();
                return;
            }
        }
    }
});

// Initialization
fetchStatus().then(() => {
    startSimulationLoop();
    // Periodically update UI details even if simulation paused, to allow canvas redrawing
    setInterval(fetchStatus, 300);
});
