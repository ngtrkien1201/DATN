// ============================================================
// 1. ROUTER VIRTUAL (SPA Navigation)
// ============================================================
const menuItems = document.querySelectorAll('.menu-item');
const pages = document.querySelectorAll('.page-content');

function updateActiveCharts(targetId) {
    if (!targetId) {
        const active = document.querySelector('.page-content:not(.hidden)');
        targetId = active ? active.id : 'overview-page';
    }
    if ((targetId === 'overview-page' || targetId === 'p-overview') && charts.overview) {
        charts.overview.update('none');
    } else if (targetId === 'live-page' || targetId === 'p-monitor') {
        if (charts.lmV) charts.lmV.update('none');
        if (charts.lmI) charts.lmI.update('none');
        if (charts.lmT) charts.lmT.update('none');
        if (charts.lmP) charts.lmP.update('none');
    } else if (targetId === 'validation-page' || targetId === 'p-validation') {
        if (charts.valVolt) charts.valVolt.update('none');
        if (charts.valSoc) charts.valSoc.update('none');
        if (charts.valErr) charts.valErr.update('none');
        if (charts.valRes) charts.valRes.update('none');
    }
}

menuItems.forEach(item => {
    item.addEventListener('click', () => {
        menuItems.forEach(i => {
            i.classList.remove('active');
            i.removeAttribute('aria-current');
        });
        item.classList.add('active');
        item.setAttribute('aria-current', 'page');
        
        pages.forEach(p => {
            p.classList.add('hidden');
            p.style.animation = 'none'; // Reset animation
        });
        
        const targetId = item.getAttribute('data-target');
        const targetPage = document.getElementById(targetId);
        if(targetPage) {
            targetPage.classList.remove('hidden');
            // Trigger reflow to restart animation
            void targetPage.offsetWidth; 
            targetPage.style.animation = 'fadeUp 0.5s cubic-bezier(0.16, 1, 0.3, 1) forwards';
            updateActiveCharts(targetId);
        }
    });
});

// ============================================================
// 2. SCENARIO SIMULATION (WHAT-IF SANDBOX)
// ============================================================
async function runWhatIf() {
    const current = document.getElementById('sim-current').value;
    const duration = document.getElementById('sim-duration').value;
    
    try {
        const res = await fetch(`/api/simulate`, { 
            method: 'POST', 
            headers: {'Content-Type': 'application/json'}, 
            body: JSON.stringify({current: parseFloat(current), duration: parseInt(duration)}) 
        });
        const data = await res.json();
        
        document.getElementById('sim-results').classList.remove('hidden');
        document.getElementById('sim-r-v').innerText = data.sim_voltage + ' V';
        document.getElementById('sim-r-soc').innerText = data.sim_soc + ' %';
        document.getElementById('sim-r-t').innerText = data.sim_temp + ' °C';
        document.getElementById('sim-r-vp').innerText = data.sim_vp + ' V';
    } catch (e) { alert("Lỗi khi chạy mô phỏng kịch bản!"); }
}

// ============================================================
// 3. CHART.JS INITIALIZATION WITH TIME AXIS
// ============================================================
let charts = {};

const commonOptions = {
    responsive: true,
    maintainAspectRatio: false,
    animation: false,
    scales: {
        x: { 
            type: 'time', 
            time: { unit: 'second', displayFormats: { second: 'HH:mm:ss' }, tooltipFormat: 'HH:mm:ss' },
            grid: { color: 'rgba(255,255,255,0.03)' },
            ticks: { color: '#64748b', font: { size: 10 } }
        },
        y: { 
            grid: { color: 'rgba(255,255,255,0.03)' }, 
            ticks: { color: '#64748b', font: { size: 10 } } 
        }
    },
    plugins: {
        legend: { display: false }
    }
};

function createChart(canvasId, datasets, isDual = false) {
    const el = document.getElementById(canvasId);
    if (!el) return null;
    
    let opt = JSON.parse(JSON.stringify(commonOptions));
    if (isDual) {
        opt.plugins.legend = { display: true, labels: { color: '#94a3b8', font: { size: 11 } } };
    }
    
    return new Chart(el.getContext('2d'), {
        type: 'line',
        data: { datasets: datasets },
        options: opt
    });
}

function initCharts() {
    // Overview
    charts.overview = createChart('c-overview', [
        { label: 'Điện áp Pack (V)', data: [], borderColor: '#60a5fa', backgroundColor: 'rgba(96,165,250,.08)', fill: true, tension: 0.35, pointRadius: 0, borderWidth: 2 }
    ]);
    
    // Live Monitoring
    charts.lmV = createChart('c-lm-v', [{ data: [], borderColor: '#3b82f6', backgroundColor: '#3b82f622', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.lmI = createChart('c-lm-i', [{ data: [], borderColor: '#f59e0b', backgroundColor: '#f59e0b22', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.lmT = createChart('c-lm-t', [{ data: [], borderColor: '#ef4444', backgroundColor: '#ef444422', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.lmP = createChart('c-lm-p', [{ data: [], borderColor: '#10b981', backgroundColor: '#10b98122', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    
    // Validation
    charts.valVolt = createChart('c-val-volt', [
        { label: 'Thực tế (V_meas)', data: [], borderColor: '#3b82f6', tension: 0.4, pointRadius: 0, borderWidth: 2 },
        { label: 'Mô hình (V_twin)', data: [], borderColor: '#10b981', tension: 0.4, pointRadius: 0, borderWidth: 2, borderDash: [6, 4] }
    ], true);
    
    charts.valSoc = createChart('c-val-soc', [
        { label: 'SOC Phần cứng', data: [], borderColor: '#3b82f6', tension: 0.4, pointRadius: 0, borderWidth: 2 },
        { label: 'SOC Digital Twin', data: [], borderColor: '#10b981', tension: 0.4, pointRadius: 0, borderWidth: 2, borderDash: [6, 4] }
    ], true);
    
    charts.valErr = createChart('c-val-err', [{ data: [], borderColor: '#ef4444', backgroundColor: '#ef444422', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.valRes = createChart('c-val-res', [{ data: [], borderColor: '#f59e0b', backgroundColor: '#f59e0b22', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
}
initCharts();

const MAX_POINTS = 60;
function pushData(chart, datasetIndex, x, y) {
    if(!chart) return;
    const data = chart.data.datasets[datasetIndex].data;
    data.push({ x: x, y: y });
    if (data.length > MAX_POINTS) data.shift();
}

// Bảng ánh xạ nhãn chuyên ngành sang song ngữ dễ hiểu
const STATUS_MODE_MAP = {
    "Discharging": "Đang xả (Discharging)",
    "Charging": "Đang sạc (Charging)",
    "Idle": "Chờ (Idle)"
};

const SYNC_STATUS_MAP = {
    "Synchronized": "Đã đồng bộ (Synchronized)",
    "Syncing": "Đang đồng bộ (Syncing)...",
    "Disconnected": "Mất kết nối (Disconnected)",
    "Connection Weak": "Tín hiệu yếu (Weak)"
};

const AI_CLASS_MAP_VN = {
    "Normal Operation": "Bình thường (Normal)",
    "Normal": "Bình thường (Normal)",
    "Over-voltage": "Quá áp (Over-voltage)",
    "Under-voltage": "Sụt áp (Under-voltage)",
    "Over-current": "Quá dòng (Over-current)",
    "Over-heat": "Quá nhiệt (Over-temperature)",
    "Cell Imbalance": "Lệch cell (Cell Imbalance)",
    "Cell-Imbalance": "Lệch cell (Cell Imbalance)"
};

const AI_STATUS_LABEL_MAP = {
    "Normal": "Bình thường (Normal)",
    "Warning": "Cảnh báo (Warning)",
    "Alert": "Nguy hiểm (Alert)"
};

const AGING_STAGE_MAP = {
    "Normal": "Bình thường (Normal)",
    "Degraded": "Suy giảm (Degraded)",
    "End of Life": "Cuối vòng đời (EOL)"
};

function formatLatency(ms) {
    if (ms === undefined || ms === null || isNaN(ms)) return "--";
    const s = ms / 1000;
    if (s < 5) return "Thời gian thực (< 5s)";
    if (s < 60) return `${s.toFixed(1)} s trước`;
    if (s < 3600) return `${Math.floor(s / 60)} phút trước`;
    return `> ${(s / 3600).toFixed(1)} giờ trước`;
}

function updateAlerts(d, timeStr) {
    const alertContainer = document.getElementById('active-alerts');
    if (!alertContainer) return;
    
    let alertsHtml = '';
    const isOnline = d.sync_metrics && d.sync_metrics.status !== "Disconnected";
    
    if (!isOnline) {
        alertsHtml += `<div class="alert-item alert-danger"><i data-lucide="wifi-off"></i> Mất kết nối telemetry với ESP32/STM32 (${timeStr}). Đang chờ dữ liệu...</div>`;
    }
    
    if (d.edge_ai && d.edge_ai.anomaly_score >= 0.7) {
        const faultName = AI_CLASS_MAP_VN[d.edge_ai.anomaly_class] || d.edge_ai.anomaly_class;
        alertsHtml += `<div class="alert-item alert-danger"><i data-lucide="alert-triangle"></i> Cảnh báo Edge AI: Phát hiện bất thường "${faultName}" (Score: ${d.edge_ai.anomaly_score.toFixed(2)}) lúc ${timeStr}.</div>`;
    } else if (d.edge_ai && d.edge_ai.anomaly_score >= 0.3) {
        alertsHtml += `<div class="alert-item alert-warning"><i data-lucide="alert-circle"></i> Chú ý: Chỉ số bất thường tăng nhẹ (${d.edge_ai.anomaly_score.toFixed(2)}). Hệ thống tiếp tục theo dõi.</div>`;
    }
    
    if (d.real.voltage > 16.8) {
        alertsHtml += `<div class="alert-item alert-danger"><i data-lucide="zap"></i> Cảnh báo quá áp Pack: ${d.real.voltage}V vượt ngưỡng an toàn 16.8V!</div>`;
    } else if (d.real.voltage < 10.0 && d.real.voltage > 0) {
        alertsHtml += `<div class="alert-item alert-danger"><i data-lucide="battery-warning"></i> Cảnh báo sụt áp sâu Pack: ${d.real.voltage}V < 10.0V! Cần ngắt tải ngay.</div>`;
    }
    
    if (d.real.soc <= 15.0 && d.real.soc > 0) {
        alertsHtml += `<div class="alert-item alert-warning"><i data-lucide="battery"></i> Dung lượng pin thấp: SOC = ${d.real.soc}%. Khuyến nghị sạc bổ sung.</div>`;
    }
    
    if (d.real.temperature >= 45.0) {
        alertsHtml += `<div class="alert-item alert-danger"><i data-lucide="thermometer"></i> Cảnh báo nhiệt độ cao: ${d.real.temperature}°C!</div>`;
    }
    
    if (alertsHtml === '') {
        alertsHtml = `<div class="alert-item alert-success"><i data-lucide="check-circle-2"></i> Hệ thống đang vận hành ổn định. Dữ liệu Digital Twin và Edge AI đồng bộ tốt (${timeStr}).</div>`;
    }
    
    alertContainer.innerHTML = alertsHtml;
    if (window.lucide) lucide.createIcons();
}

// ============================================================
// 4. DATA POLLING & UI UPDATE
// ============================================================
let isFetching = false;
let activeDemoScenario = 'off';

const DEMO_SCENARIOS = {
    normal: { label: 'Bình thường', cells: [3.82, 3.81, 3.83, 3.82], current: 0.45, temperature: 31, soc: 72, ai: 0, cmd: 'NORMAL' },
    imbalance: { label: 'Lệch cell', cells: [3.68, 3.69, 3.94, 3.68], current: 0.45, temperature: 32, soc: 68, ai: 5, cmd: 'IMBALANCE' },
    overvoltage: { label: 'Quá áp', cells: [3.82, 3.81, 4.25, 3.82], current: 0.35, temperature: 33, soc: 84, ai: 1, cmd: 'OV' },
    undervoltage: { label: 'Sụt áp', cells: [3.65, 3.67, 2.90, 3.66], current: 0.40, temperature: 32, soc: 18, ai: 2, cmd: 'UV' },
    overcurrent: { label: 'Quá dòng', cells: [3.78, 3.77, 3.78, 3.79], current: 3.4, temperature: 36, soc: 55, ai: 3, cmd: 'OC' },
    overtemperature: { label: 'Quá nhiệt', cells: [3.78, 3.77, 3.78, 3.79], current: 0.50, temperature: 56, soc: 55, ai: 4, cmd: 'OT' }
};

function applyDemoScenario(data) {
    const scenario = DEMO_SCENARIOS[activeDemoScenario];
    if (!scenario) return data;

    const d = structuredClone(data);
    const packVoltage = scenario.cells.reduce((sum, value) => sum + value, 0);
    const power = scenario.current * packVoltage;
    const aiNames = ['Normal Operation', 'Over-voltage', 'Under-voltage', 'Over-current', 'Over-heat', 'Cell Imbalance'];
    d.real = { ...d.real, voltage: +packVoltage.toFixed(2), current: scenario.current, power: +power.toFixed(2), temperature: scenario.temperature, soc: scenario.soc, status: 'Discharging' };
    d.cells = { ...d.cells, cv: scenario.cells, csoc: scenario.cells.map(() => scenario.soc), cai_class: scenario.cells.map((_, i) => scenario.ai === 5 ? (i === 2 ? 5 : 0) : scenario.ai) };
    d.edge_ai = { ...d.edge_ai, status: 'Running (DEMO)', status_label: scenario.ai === 0 ? 'Normal' : 'Alert', anomaly_class: aiNames[scenario.ai], anomaly_score: scenario.ai === 0 ? 0.04 : 0.96, confidence: scenario.ai === 0 ? 96 : 96, inference_time: 1.2 };
    d.twin = { ...d.twin, voltage: +(packVoltage - 0.08).toFixed(2), terminal_voltage: +(packVoltage - 0.08).toFixed(2), ocv: +(packVoltage + 0.12).toFixed(2), soc: Math.max(0, scenario.soc - 1.5), remaining_capacity: +(2.2 * scenario.soc / 100).toFixed(2) };
    d.errors = { ...d.errors, voltage: 0.5, voltage_mv: 80 };
    d.sync_metrics = { ...d.sync_metrics, status: 'Synchronized', latency_ms: 120 };
    d.demo_mode = scenario.label;
    return d;
}

async function selectDemoScenario(mode) {
    activeDemoScenario = mode;
    document.querySelectorAll('.demo-mode-btn').forEach(button => {
        button.classList.toggle('active', button.dataset.demo === mode);
    });
    const scenario = DEMO_SCENARIOS[mode];
    const status = document.getElementById('demo-mode-status');
    if (status) status.textContent = scenario ? `Đang trình diễn: ${scenario.label}. Dữ liệu trên dashboard là mô phỏng.` : 'Đang hiển thị dữ liệu thực.';

    // Gửi lệnh đồng bộ phần cứng (MOSFET Quạt STM32, LED, Còi ESP32)
    const cmdMode = scenario ? scenario.cmd : 'OFF';
    try {
        const response = await fetch('/api/command', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({command: `DEMO:${cmdMode}`})
        });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        if (status && scenario) status.textContent += ' Lệnh đồng bộ Quạt/Đèn/Còi đã được gửi tới mạch phần cứng.';
    } catch (error) {
        console.warn('Demo command could not be queued:', error);
        if (status && scenario) status.textContent += ' Chưa kết nối gateway; dashboard vẫn đang mô phỏng.';
    }
    updateTwin();
}

document.querySelectorAll('.demo-mode-btn').forEach(button => {
    button.addEventListener('click', () => selectDemoScenario(button.dataset.demo));
});

async function updateTwin() {
    if (isFetching) return;
    isFetching = true;
    try {
        const res = await fetch(`/api/digital-twin`);
        if (!res.ok) throw new Error(`Dashboard API returned ${res.status}`);
        const d = applyDemoScenario(await res.json());
        
        const now = new Date();
        const timeStr = now.toLocaleTimeString();

        // ------------------ GLOBAL STATUS BAR ------------------
        document.getElementById('gb-time').innerText = timeStr;
        const isOnline = d.sync_metrics.status !== "Disconnected";
        document.getElementById('gb-sys').innerText = isOnline ? "Đang nhận dữ liệu" : "Mất tín hiệu";
        document.getElementById('gb-sys').className = isOnline ? "text-green" : "text-red";
        
        document.getElementById('gb-esp').innerText = isOnline ? "Đã kết nối" : "Mất kết nối";
        document.getElementById('gb-esp').className = isOnline ? "" : "text-red";
        document.getElementById('gb-mqtt').innerText = "Đã kết nối";
        document.getElementById('gb-mqtt').className = "text-green";
        
        document.getElementById('gb-mode').innerText = d.demo_mode ? `DEMO · ${d.demo_mode}` : (STATUS_MODE_MAP[d.real.status] || d.real.status);
        
        // ------------------ PAGE 1: SYSTEM OVERVIEW ------------------
        if(document.getElementById('ov-v')) document.getElementById('ov-v').innerText = d.real.voltage + ' V';
        if(document.getElementById('ov-i')) document.getElementById('ov-i').innerText = d.real.current + ' A';
        if(document.getElementById('ov-t')) document.getElementById('ov-t').innerText = d.real.temperature + ' °C';
        if(document.getElementById('ov-p')) document.getElementById('ov-p').innerText = (d.real.power || (d.real.voltage * d.real.current).toFixed(3)) + ' W';
        if(document.getElementById('ov-e')) document.getElementById('ov-e').innerText = (d.real.energy || 0.0) + ' Wh';
        
        if(document.getElementById('ov-soc')) document.getElementById('ov-soc').innerText = d.real.soc + ' %';
        if(document.getElementById('ov-soh')) document.getElementById('ov-soh').innerText = d.real.soh + ' %';
        if(document.getElementById('ov-cycle')) document.getElementById('ov-cycle').innerText = d.twin.cycle_count;
        
        if(document.getElementById('ov-tv')) document.getElementById('ov-tv').innerText = d.twin.terminal_voltage + ' V';
        if(document.getElementById('ov-tsoc')) document.getElementById('ov-tsoc').innerText = d.twin.soc + ' %';
        if(document.getElementById('ov-err')) document.getElementById('ov-err').innerText = (d.errors.voltage_mv !== undefined ? d.errors.voltage_mv : (d.errors.voltage * 1000).toFixed(1)) + ' mV';
        if(document.getElementById('ov-sync')) document.getElementById('ov-sync').innerText = SYNC_STATUS_MAP[d.sync_metrics.status] || d.sync_metrics.status;
        
        const anomalyRaw = d.edge_ai ? d.edge_ai.anomaly_class : "Normal Operation";
        if(document.getElementById('ov-anomaly')) document.getElementById('ov-anomaly').innerText = AI_CLASS_MAP_VN[anomalyRaw] || anomalyRaw;
        if(document.getElementById('ov-ascore')) document.getElementById('ov-ascore').innerText = d.edge_ai ? d.edge_ai.anomaly_score : "0.00";
        if(document.getElementById('ov-atime')) document.getElementById('ov-atime').innerText = (d.edge_ai ? d.edge_ai.inference_time : 1) + " ms";
        
        // ------------------ PAGE 1: CELLS DATA (4S) ------------------
        if (d.cells) {
            for (let i = 0; i < 4; i++) {
                const c_v = d.cells.cv[i].toFixed(2);
                const c_soc = d.cells.csoc[i];
                const c_ai = d.cells.cai_class[i];
                let colorClass = "text-green";
                if (c_ai === 5) {
                    colorClass = "text-orange";
                } else if (c_ai > 0) {
                    colorClass = "text-red";
                }
                
                const cellEl = document.getElementById(`ov-c${i+1}`);
                if (cellEl) {
                    const c_status = c_ai === 0 ? "Ổn định" : (`Lỗi C${c_ai}`);
                    cellEl.innerHTML = `<span class="${colorClass}">${c_v} V | ${c_soc}% | ${c_status}</span>`;
                }
            }
        }
        
        // ------------------ PAGE 2: LIVE MONITORING ------------------
        if(document.getElementById('lm-v')) document.getElementById('lm-v').innerHTML = `${d.real.voltage} <small>V</small>`;
        if(document.getElementById('lm-i')) document.getElementById('lm-i').innerHTML = `${d.real.current} <small>A</small>`;
        if(document.getElementById('lm-p')) document.getElementById('lm-p').innerHTML = `${d.real.power || (d.real.voltage * d.real.current).toFixed(3)} <small>W</small>`;
        if(document.getElementById('lm-soc')) document.getElementById('lm-soc').innerHTML = `${d.real.soc} <small>%</small>`;
        
        const monitorPage = document.getElementById('live-page');
        if(monitorPage && !monitorPage.classList.contains('hidden')) {
            const liveTb = document.getElementById('live-table-body');
            if(liveTb) {
                const row = document.createElement('tr');
                row.innerHTML = `<td>${timeStr}</td><td>${d.real.voltage} V</td><td>${d.real.current} A</td><td>${d.real.temperature} °C</td><td>${d.real.power || 0} W</td><td>${d.real.energy || 0} Wh</td><td class="text-green">${d.real.soc} %</td>`;
                liveTb.insertBefore(row, liveTb.firstChild);
                if(liveTb.children.length > 50) liveTb.removeChild(liveTb.lastChild);
            }
        }
        
        // ------------------ PAGE 3: DIGITAL TWIN ------------------
        if(document.getElementById('dt-pb-v')) document.getElementById('dt-pb-v').innerText = d.real.voltage + ' V';
        if(document.getElementById('dt-pb-i')) document.getElementById('dt-pb-i').innerText = d.real.current + ' A';
        if(document.getElementById('dt-pb-soc')) document.getElementById('dt-pb-soc').innerText = d.real.soc + ' %';
        if(document.getElementById('dt-pb-fill')) document.getElementById('dt-pb-fill').style.height = `${d.real.soc}%`;
        
        const syncStatusStr = SYNC_STATUS_MAP[d.sync_metrics.status] || d.sync_metrics.status;
        if(document.getElementById('dt-sync-status')) document.getElementById('dt-sync-status').innerText = syncStatusStr;
        if(document.getElementById('dt-sync-pv')) document.getElementById('dt-sync-pv').innerText = d.real.voltage + ' V';
        if(document.getElementById('dt-sync-tv')) document.getElementById('dt-sync-tv').innerText = d.twin.terminal_voltage + ' V';
        
        let residual = Math.abs(d.real.voltage - d.twin.terminal_voltage).toFixed(3);
        if(document.getElementById('dt-sync-res')) document.getElementById('dt-sync-res').innerText = residual + ' V';
        if(document.getElementById('dt-sync-err')) document.getElementById('dt-sync-err').innerText = d.errors.voltage + ' %';
        if(document.getElementById('dt-sync-age')) document.getElementById('dt-sync-age').innerText = formatLatency(d.sync_metrics.latency_ms);
        
        if(document.getElementById('dt-tb-ocv')) document.getElementById('dt-tb-ocv').innerText = d.twin.ocv + ' V';
        if(document.getElementById('dt-tb-tv')) document.getElementById('dt-tb-tv').innerText = d.twin.terminal_voltage + ' V';
        if(document.getElementById('dt-tb-soc')) document.getElementById('dt-tb-soc').innerText = d.twin.soc + ' %';
        if(document.getElementById('dt-tb-fill')) document.getElementById('dt-tb-fill').style.height = `${d.twin.soc}%`;
        
        if(document.getElementById('dt-p-r0')) document.getElementById('dt-p-r0').innerText = d.model_parameters.R0_mOhm + ' mΩ';
        if(document.getElementById('dt-p-r1')) document.getElementById('dt-p-r1').innerText = (d.model_parameters.R1 * 1000).toFixed(1) + ' mΩ';
        if(document.getElementById('dt-p-c1')) document.getElementById('dt-p-c1').innerText = d.model_parameters.C1 + ' F';
        if(document.getElementById('dt-p-vp')) document.getElementById('dt-p-vp').innerText = d.twin.polarization_voltage + ' V';
        
        // ------------------ PAGE 4: EDGE AI ------------------
        if(d.edge_ai) {
            const aiAvailable = d.edge_ai.status !== 'Waiting for Data' && d.edge_ai.inference_time > 0;
            const aiScore = aiAvailable ? d.edge_ai.anomaly_score : 0;
            const scoreEl = document.getElementById('ai-score');
            const scoreBar = document.getElementById('ai-score-bar');
            if (scoreEl) scoreEl.innerText = aiAvailable ? aiScore.toFixed(2) : '--';
            
            // Dynamic color for score
            let colorVar = 'var(--emerald)';
            if (aiScore >= 0.7) {
                colorVar = 'var(--rose)';
            } else if (aiScore >= 0.3) {
                colorVar = 'var(--orange)';
            }
            
            if (scoreEl) {
                scoreEl.style.color = colorVar;
                scoreEl.style.textShadow = `0 0 20px ${colorVar}66`;
            }
            if (scoreBar) {
                scoreBar.style.width = aiAvailable ? `${Math.min(100, Math.max(0, aiScore * 100))}%` : '0%';
                scoreBar.style.background = colorVar;
            }

            const statusEl = document.getElementById('ai-status');
            if (statusEl) {
                const rawLabel = d.edge_ai.status_label || d.edge_ai.status;
                statusEl.innerText = aiAvailable ? (AI_STATUS_LABEL_MAP[rawLabel] || rawLabel) : 'Đang chờ dữ liệu';
                if (d.edge_ai.status_label === 'Normal') {
                    statusEl.className = 'text-green';
                } else if (d.edge_ai.status_label === 'Warning') {
                    statusEl.className = 'text-orange';
                } else if (!aiAvailable) {
                    statusEl.className = 'text-muted';
                } else {
                    statusEl.className = 'text-red';
                }
            }

            const rawAiClass = d.edge_ai.anomaly_class;
            if(document.getElementById('ai-class')) {
                document.getElementById('ai-class').innerText = aiAvailable ? (AI_CLASS_MAP_VN[rawAiClass] || rawAiClass) : '--';
            }
            
            const conf = d.edge_ai.confidence;
            if(document.getElementById('ai-conf')) document.getElementById('ai-conf').innerText = aiAvailable ? conf + '%' : '--';
            const confBar = document.getElementById('ai-conf-bar');
            if (confBar) {
                confBar.style.width = aiAvailable ? `${conf}%` : '0%';
            }
            
            if(document.getElementById('ai-time')) document.getElementById('ai-time').innerText = aiAvailable ? d.edge_ai.inference_time + ' ms' : '--';
            
        }
        // ------------------ PAGE 5: VALIDATION ------------------
        if(d.validation) {
            if(document.getElementById('val-v-mae')) document.getElementById('val-v-mae').innerHTML = `${d.validation.v_mae} <small>V</small>`;
            if(document.getElementById('val-v-rmse')) document.getElementById('val-v-rmse').innerHTML = `${d.validation.v_rmse} <small>V</small>`;
            if(document.getElementById('val-v-max')) document.getElementById('val-v-max').innerHTML = `${d.validation.v_max_err} <small>V</small>`;
            if(document.getElementById('val-soc-mae')) document.getElementById('val-soc-mae').innerHTML = `${d.validation.soc_mae} <small>%</small>`;
        }
        
        // ------------------ PAGE 6: RUL ------------------
        if(document.getElementById('rul-cycles')) document.getElementById('rul-cycles').innerText = d.aging_model.rul_cycles.split(' ')[0];
        if(document.getElementById('rul-conf')) document.getElementById('rul-conf').innerText = d.aging_model.rul_confidence_range || 'N/A';
        if(document.getElementById('rul-soh-cap')) document.getElementById('rul-soh-cap').innerText = d.twin.soh + ' %';
        if(document.getElementById('rul-soh-res')) document.getElementById('rul-soh-res').innerText = (100 - parseFloat(d.aging_model.resistance_growth)).toFixed(1) + ' %';
        if(document.getElementById('rul-fade')) document.getElementById('rul-fade').innerText = d.aging_model.capacity_fade;
        if(document.getElementById('rul-growth')) document.getElementById('rul-growth').innerText = d.aging_model.resistance_growth;
        if(document.getElementById('rul-est-cap')) document.getElementById('rul-est-cap').innerText = d.twin.remaining_capacity + ' Ah';
        if(document.getElementById('rul-stage')) {
            const rawStage = d.aging_model.aging_stage;
            const badgeEl = document.getElementById('rul-stage');
            badgeEl.innerText = AGING_STAGE_MAP[rawStage] || rawStage;
            if (rawStage === 'Normal') badgeEl.className = 'status-badge badge-green';
            else if (rawStage === 'Degraded') badgeEl.className = 'status-badge badge-orange';
            else badgeEl.className = 'status-badge badge-red';
        }

        // Cập nhật cảnh báo & thông báo động
        updateAlerts(d, timeStr);

        // ------------------ UPDATE CHARTS ------------------
        let timestamp = now.getTime();
        
        pushData(charts.overview, 0, timestamp, d.real.voltage);
        pushData(charts.lmV, 0, timestamp, d.real.voltage);
        pushData(charts.lmI, 0, timestamp, d.real.current);
        pushData(charts.lmT, 0, timestamp, d.real.temperature);
        pushData(charts.lmP, 0, timestamp, d.real.power || (d.real.voltage * d.real.current));
        
        pushData(charts.valVolt, 0, timestamp, d.real.voltage);
        pushData(charts.valVolt, 1, timestamp, d.twin.terminal_voltage);
        pushData(charts.valSoc, 0, timestamp, d.real.soc);
        pushData(charts.valSoc, 1, timestamp, d.twin.soc);
        pushData(charts.valErr, 0, timestamp, residual);
        pushData(charts.valRes, 0, timestamp, d.model_parameters.R0_mOhm);
        
        // Chỉ cập nhật đồ thị trên trang đang hiển thị để tối ưu hiệu năng
        updateActiveCharts();
    } catch (e) {
        console.error("Fetch error: ", e);
        const telemetryStatus = document.getElementById('gb-sys');
        const backendStatus = document.getElementById('gb-mqtt');
        if (telemetryStatus) {
            telemetryStatus.innerText = "Chưa có dữ liệu";
            telemetryStatus.className = "text-orange";
        }
        if (backendStatus) {
            backendStatus.innerText = "Không kết nối được";
            backendStatus.className = "text-red";
        }
    } finally {
        isFetching = false;
    }
}

// Khởi tạo icon Lucide 1 lần duy nhất khi tải trang
document.addEventListener('DOMContentLoaded', () => {
    if (window.lucide) lucide.createIcons();
});

// Polling mỗi 1.5 giây
setInterval(() => {
    if (!isFetching) updateTwin();
}, 1500);
updateTwin();

// ============================================================
// 5. SYSTEM CONFIGURATION
// ============================================================
async function updateBatteryConfig() {
    const selectEl = document.getElementById('config-battery-type');
    const batteryId = selectEl.value;
    const batteryText = selectEl.options[selectEl.selectedIndex].text;
    const cmdStr = `SET_PIN:${batteryId}`;
    
    // Parse dung lượng từ text (VD: "Generic 18650 3.0Ah" -> 3.0)
    let capacity = 2.2;
    const match = batteryText.match(/(\d+(?:\.\d+)?)\s*Ah/i);
    if(match) capacity = parseFloat(match[1]);

    try {
        const res = await fetch('/api/command', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ command: cmdStr, capacity: capacity })
        });
        
        if(res.ok) {
            alert(`Đã gửi cấu hình thành công! Loại pin đã chuyển sang: ${batteryText}.`);
        } else {
            alert("Lỗi khi gửi cấu hình xuống thiết bị.");
        }
    } catch (e) {
        console.error("Config error:", e);
        alert("Lỗi kết nối khi gửi cấu hình.");
    }
}

async function resetBatterySOH() {
    if(!confirm("CẢNH BÁO: Hành động này sẽ xóa bộ nhớ Flash trên STM32 và khôi phục tuổi thọ SOH về 100%. Bạn có chắc chắn mình đã lắp 4 viên pin mới vào không?")) {
        return;
    }
    
    try {
        const res = await fetch('/api/reset_soh', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'}
        });
        
        if(res.ok) {
            alert("Lệnh RESET SOH đã được chuyển xuống phần cứng thành công!");
        } else {
            alert("Lỗi khi gửi lệnh reset.");
        }
    } catch (e) {
        console.error("Reset error:", e);
        alert("Lỗi kết nối tới Server.");
    }
}
