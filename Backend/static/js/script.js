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
    const current = Number(document.getElementById('sim-current').value);
    const duration = Number(document.getElementById('sim-duration').value);
    const button = document.getElementById('sim-run');
    const message = document.getElementById('sim-message');
    const results = document.getElementById('sim-results');
    if (!Number.isFinite(current) || !Number.isFinite(duration) || duration <= 0) {
        message.innerText = 'Hãy nhập dòng điện hợp lệ và thời gian lớn hơn 0.';
        message.className = 'text-sm text-orange';
        return;
    }
    button.disabled = true;
    message.innerText = 'Đang tính toán…';
    message.className = 'text-sm text-muted';
    try {
        const res = await fetch('/api/simulate', {
            method: 'POST', 
            headers: {'Content-Type': 'application/json'}, 
            body: JSON.stringify({current, duration})
        });
        const data = await res.json();
        if (!res.ok || data.error) throw new Error(data.error || `HTTP ${res.status}`);
        results.classList.remove('hidden');
        document.getElementById('sim-r-v').innerText = data.sim_voltage + ' V';
        document.getElementById('sim-r-soc').innerText = data.sim_soc + ' %';
        document.getElementById('sim-r-t').innerText = data.sim_temp + ' °C';
        document.getElementById('sim-r-vp').innerText = data.sim_vp + ' V';
        message.innerText = 'Đã tính xong kịch bản.';
        message.className = 'text-sm text-green';
    } catch (e) {
        results.classList.add('hidden');
        message.innerText = e.message === 'Twin Engine chưa khởi động'
            ? 'Cần có dữ liệu pin trước khi chạy mô phỏng.'
            : 'Không thể chạy mô phỏng. Hãy kiểm tra kết nối rồi thử lại.';
        message.className = 'text-sm text-red';
    } finally {
        button.disabled = false;
    }
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
    const showChartFallback = () => {
        const message = document.createElement('p');
        message.className = 'chart-fallback';
        message.textContent = 'Không tải được thư viện biểu đồ. Hãy kiểm tra kết nối mạng.';
        el.insertAdjacentElement('afterend', message);
        el.hidden = true;
    };
    if (!window.Chart) {
        showChartFallback();
        return null;
    }
    
    let opt = JSON.parse(JSON.stringify(commonOptions));
    if (isDual) {
        opt.plugins.legend = { display: true, labels: { color: '#94a3b8', font: { size: 11 } } };
    }
    
    try {
        return new Chart(el.getContext('2d'), {
            type: 'line',
            data: { datasets: datasets },
            options: opt
        });
    } catch (error) {
        console.error(`Chart initialization failed (${canvasId}):`, error);
        showChartFallback();
        return null;
    }
}

function initCharts() {
    // Overview
    charts.overview = createChart('c-overview', [
        { label: 'Điện áp bộ pin (V)', data: [], borderColor: '#60a5fa', backgroundColor: 'rgba(96,165,250,.08)', fill: true, tension: 0.35, pointRadius: 0, borderWidth: 2 }
    ]);
    
    // Live Monitoring
    charts.lmV = createChart('c-lm-v', [{ data: [], borderColor: '#3b82f6', backgroundColor: '#3b82f622', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.lmI = createChart('c-lm-i', [{ data: [], borderColor: '#f59e0b', backgroundColor: '#f59e0b22', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.lmT = createChart('c-lm-t', [{ data: [], borderColor: '#ef4444', backgroundColor: '#ef444422', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.lmP = createChart('c-lm-p', [{ data: [], borderColor: '#10b981', backgroundColor: '#10b98122', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    
    // Validation
    charts.valVolt = createChart('c-val-volt', [
        { label: 'Đo thực tế', data: [], borderColor: '#3b82f6', tension: 0.4, pointRadius: 0, borderWidth: 2 },
        { label: 'Mô hình số', data: [], borderColor: '#10b981', tension: 0.4, pointRadius: 0, borderWidth: 2, borderDash: [6, 4] }
    ], true);
    
    charts.valSoc = createChart('c-val-soc', [
        { label: 'Mức pin đo được', data: [], borderColor: '#3b82f6', tension: 0.4, pointRadius: 0, borderWidth: 2 },
        { label: 'Mức pin mô hình số', data: [], borderColor: '#10b981', tension: 0.4, pointRadius: 0, borderWidth: 2, borderDash: [6, 4] }
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
    "Discharging": "Đang xả",
    "Charging": "Đang sạc",
    "Idle": "Chờ"
};

const SYNC_STATUS_MAP = {
    "Synchronized": "Đã đồng bộ",
    "Syncing": "Đang đồng bộ…",
    "Disconnected": "Mất kết nối",
    "Connection Weak": "Tín hiệu yếu"
};

const AI_CLASS_MAP_VN = {
    "Normal Operation": "Bình thường",
    "Normal": "Bình thường",
    "Over-voltage": "Quá áp",
    "Under-voltage": "Thấp áp",
    "Over-current": "Quá dòng",
    "Over-heat": "Quá nhiệt",
    "Cell Imbalance": "Lệch điện áp cell",
    "Cell-Imbalance": "Lệch điện áp cell"
};

const AI_STATUS_LABEL_MAP = {
    "Normal": "Bình thường",
    "Warning": "Cảnh báo",
    "Alert": "Nguy hiểm"
};

const AGING_STAGE_MAP = {
    "Normal": "Bình thường",
    "Degraded": "Suy giảm",
    "End of Life": "Cuối vòng đời"
};

function formatReceiveInterval(ms) {
    if (ms === undefined || ms === null || isNaN(ms)) return "--";
    return `${(ms / 1000).toFixed(1)} s`;
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
let lastChartIteration = null;
let lastLiveRowIteration = null;

async function updateTwin() {
    if (isFetching) return;
    isFetching = true;
    try {
        const res = await fetch(`/api/digital-twin`);
        if (!res.ok) throw new Error(`Dashboard API returned ${res.status}`);
        const d = await res.json();
        
        const now = new Date();
        const timeStr = now.toLocaleTimeString();

        // ------------------ GLOBAL STATUS BAR ------------------
        if (d.metadata?.twin_id) document.getElementById('gb-id').innerText = d.metadata.twin_id;
        document.getElementById('gb-time').innerText = d.sync_metrics.last_update || timeStr;
        const isOnline = d.sync_metrics.status !== "Disconnected";
        document.getElementById('gb-sys').innerText = isOnline ? "Đang nhận dữ liệu" : "Mất tín hiệu";
        document.getElementById('gb-sys').className = isOnline ? "text-green" : "text-red";
        
        document.getElementById('gb-esp').innerText = isOnline ? "Đã kết nối" : "Mất kết nối";
        document.getElementById('gb-esp').className = isOnline ? "" : "text-red";
        document.getElementById('gb-mqtt').innerText = "Đã kết nối";
        document.getElementById('gb-mqtt').className = "text-green";
        
        document.getElementById('gb-mode').innerText = STATUS_MODE_MAP[d.real.status] || d.real.status;

        // Do not present the model's zero-initialized state as real measurements.
        if (Number(d.diagnostics?.model_iteration || 0) === 0) {
            document.getElementById('gb-mode').innerText = 'Chưa có dữ liệu';
            [
                'ov-v', 'ov-i', 'ov-t', 'ov-p', 'ov-soc', 'ov-soh', 'ov-cycle',
                'ov-c1', 'ov-c2', 'ov-c3', 'ov-c4', 'ov-tv', 'ov-tsoc', 'ov-err',
                'dt-pb-v', 'dt-pb-i', 'dt-pb-soc', 'dt-sync-pv', 'dt-sync-tv',
                'dt-sync-res', 'dt-sync-err', 'dt-sync-age', 'dt-tb-ocv', 'dt-tb-tv',
                'dt-tb-soc', 'dt-p-r0', 'dt-p-r1', 'dt-p-c1', 'dt-p-vp',
                'val-v-mae', 'val-v-rmse', 'val-v-max', 'val-soc-mae',
                'rul-cycles', 'rul-soh-cap', 'rul-soh-res', 'rul-fade', 'rul-growth', 'rul-est-cap'
            ].forEach(id => {
                const element = document.getElementById(id);
                if (element) element.innerText = '--';
            });
            ['dt-pb-fill', 'dt-tb-fill'].forEach(id => {
                const element = document.getElementById(id);
                if (element) element.style.height = '0%';
            });
            ['ov-anomaly', 'ov-ascore', 'ov-atime', 'ai-score', 'ai-class', 'ai-time', 'ai-conf', 'rul-stage']
                .forEach(id => {
                    const element = document.getElementById(id);
                    if (element) element.innerText = '--';
                });
            const aiStatus = document.getElementById('ai-status');
            if (aiStatus) aiStatus.innerText = 'Đang chờ dữ liệu';
            const stageBadge = document.getElementById('rul-stage');
            if (stageBadge) stageBadge.className = 'status-badge badge-blue';
            const twinStatus = document.getElementById('dt-sync-status');
            if (twinStatus) twinStatus.innerText = 'Chưa có dữ liệu đo';
            const alerts = document.getElementById('active-alerts');
            if (alerts) alerts.innerHTML = '<div class="alert-item alert-info"><i data-lucide="info"></i> Chưa nhận được dữ liệu từ bộ pin.</div>';
            if (window.lucide) lucide.createIcons();
            return;
        }

        document.getElementById('data-iterations').innerText = d.diagnostics.model_iteration;
        document.getElementById('data-v-mae').innerText = `${d.validation.v_mae} V`;
        document.getElementById('data-accuracy').innerText = d.diagnostics.ecm_accuracy;
        document.getElementById('data-sync-rate').innerText = `${d.sync_metrics.rate} %`;
        
        // ------------------ PAGE 1: SYSTEM OVERVIEW ------------------
        if(document.getElementById('ov-v')) document.getElementById('ov-v').innerText = d.real.voltage + ' V';
        if(document.getElementById('ov-i')) document.getElementById('ov-i').innerText = d.real.current + ' A';
        if(document.getElementById('ov-t')) document.getElementById('ov-t').innerText = d.real.temperature + ' °C';
        if(document.getElementById('ov-p')) document.getElementById('ov-p').innerText = (d.real.power || (d.real.voltage * d.real.current).toFixed(3)) + ' W';
        if(document.getElementById('ov-soc')) document.getElementById('ov-soc').innerText = d.real.soc + ' %';
        if(document.getElementById('ov-soh')) document.getElementById('ov-soh').innerText = d.real.soh + ' %';
        if(document.getElementById('ov-cycle')) document.getElementById('ov-cycle').innerText = d.twin.cycle_count;
        
        if(document.getElementById('ov-tv')) document.getElementById('ov-tv').innerText = d.twin.terminal_voltage + ' V';
        if(document.getElementById('ov-tsoc')) document.getElementById('ov-tsoc').innerText = d.twin.soc + ' %';
        if(document.getElementById('ov-err')) document.getElementById('ov-err').innerText = (d.errors.voltage_mv !== undefined ? d.errors.voltage_mv : (d.errors.voltage * 1000).toFixed(1)) + ' mV';
        if(document.getElementById('ov-sync')) document.getElementById('ov-sync').innerText = SYNC_STATUS_MAP[d.sync_metrics.status] || d.sync_metrics.status;
        
        const anomalyRaw = d.edge_ai ? d.edge_ai.anomaly_class : "Normal Operation";
        if(document.getElementById('ov-anomaly')) document.getElementById('ov-anomaly').innerText = AI_CLASS_MAP_VN[anomalyRaw] || anomalyRaw;
        if(document.getElementById('ov-ascore')) document.getElementById('ov-ascore').innerText = d.edge_ai && d.edge_ai.inference_time > 0 ? d.edge_ai.anomaly_score : "--";
        if(document.getElementById('ov-atime')) document.getElementById('ov-atime').innerText = d.edge_ai && d.edge_ai.inference_time > 0 ? d.edge_ai.inference_time + " ms" : "--";
        
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
        const modelIteration = Number(d.diagnostics?.model_iteration || 0);
        if(monitorPage && !monitorPage.classList.contains('hidden') && modelIteration !== lastLiveRowIteration) {
            const liveTb = document.getElementById('live-table-body');
            if(liveTb) {
                liveTb.querySelector('.empty-row')?.remove();
                const row = document.createElement('tr');
                const power = d.real.power ?? (d.real.voltage * d.real.current).toFixed(3);
                const energy = d.real.energy ?? '--';
                row.innerHTML = `<td>${timeStr}</td><td>${d.real.voltage} V</td><td>${d.real.current} A</td><td>${d.real.temperature} °C</td><td>${power} W</td><td>${energy}${energy === '--' ? '' : ' Wh'}</td><td class="text-green">${d.real.soc} %</td>`;
                liveTb.insertBefore(row, liveTb.firstChild);
                if(liveTb.children.length > 50) liveTb.removeChild(liveTb.lastChild);
                lastLiveRowIteration = modelIteration;
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
        if(document.getElementById('dt-sync-age')) document.getElementById('dt-sync-age').innerText = formatReceiveInterval(d.sync_metrics.latency_ms);
        
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
        if (modelIteration !== lastChartIteration) {
            const timestamp = now.getTime();
            pushData(charts.overview, 0, timestamp, d.real.voltage);
            pushData(charts.lmV, 0, timestamp, d.real.voltage);
            pushData(charts.lmI, 0, timestamp, d.real.current);
            pushData(charts.lmT, 0, timestamp, d.real.temperature);
            pushData(charts.lmP, 0, timestamp, d.real.power ?? (d.real.voltage * d.real.current));
            pushData(charts.valVolt, 0, timestamp, d.real.voltage);
            pushData(charts.valVolt, 1, timestamp, d.twin.terminal_voltage);
            pushData(charts.valSoc, 0, timestamp, d.real.soc);
            pushData(charts.valSoc, 1, timestamp, d.twin.soc);
            pushData(charts.valErr, 0, timestamp, residual);
            pushData(charts.valRes, 0, timestamp, d.model_parameters.R0_mOhm);
            lastChartIteration = modelIteration;
        }
        
        // Chỉ cập nhật đồ thị trên trang đang hiển thị để tối ưu hiệu năng
        updateActiveCharts();
    } catch (e) {
        console.error("Fetch error: ", e);
        const telemetryStatus = document.getElementById('gb-sys');
        const gatewayStatus = document.getElementById('gb-esp');
        const backendStatus = document.getElementById('gb-mqtt');
        if (telemetryStatus) {
            telemetryStatus.innerText = "Chưa có dữ liệu";
            telemetryStatus.className = "text-orange";
        }
        if (gatewayStatus) {
            gatewayStatus.innerText = "Mất kết nối";
            gatewayStatus.className = "text-red";
        }
        if (backendStatus) {
            backendStatus.innerText = "Không kết nối được";
            backendStatus.className = "text-red";
        }
        const twinStatus = document.getElementById('dt-sync-status');
        if (twinStatus) twinStatus.innerText = "Không lấy được dữ liệu mới";
        const aiStatus = document.getElementById('ai-status');
        if (aiStatus) {
            aiStatus.innerText = "Đang chờ dữ liệu";
            aiStatus.className = "text-muted";
        }
        const alerts = document.getElementById('active-alerts');
        if (alerts) alerts.innerHTML = '<div class="alert-item alert-danger"><i data-lucide="wifi-off"></i> Không thể lấy dữ liệu mới từ máy chủ.</div>';
        if (window.lucide) lucide.createIcons();
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

const AI_HISTORY_LABELS = {
    1: 'Quá áp',
    2: 'Thấp áp',
    3: 'Quá dòng',
    4: 'Quá nhiệt',
    5: 'Lệch điện áp cell'
};

function appendTableRow(table, values, className = '') {
    const row = document.createElement('tr');
    if (className) row.className = className;
    values.forEach(value => {
        const cell = document.createElement('td');
        cell.textContent = value ?? '--';
        row.appendChild(cell);
    });
    table.appendChild(row);
}

function appendEmptyTableRow(table, columnCount, message) {
    const row = document.createElement('tr');
    row.className = 'empty-row';
    const cell = document.createElement('td');
    cell.colSpan = columnCount;
    cell.textContent = message;
    row.appendChild(cell);
    table.appendChild(row);
}

function formatHistoryTime(value) {
    if (!value) return '--';
    const date = new Date(value);
    return Number.isNaN(date.getTime()) ? String(value) : date.toLocaleString('vi-VN');
}

async function loadHistory() {
    try {
        const response = await fetch('/api/history');
        if (!response.ok) throw new Error(`History API returned ${response.status}`);
        const records = await response.json();
        const historyBody = document.getElementById('data-history-body');
        const anomalyBody = document.getElementById('ai-history-body');
        if (!Array.isArray(records)) throw new Error('Invalid history response');

        historyBody.replaceChildren();
        records.slice(-20).reverse().forEach(record => {
            const flags = record.fault_flags || 'NORMAL';
            appendTableRow(historyBody, [
                formatHistoryTime(record.timestamp),
                `${record.voltage ?? '--'} V`,
                `${record.current ?? '--'} A`,
                `${record.temperature ?? '--'} °C`,
                `${record.soc ?? '--'} %`,
                flags === 'NORMAL' ? 'Ổn định' : flags
            ], flags === 'NORMAL' ? '' : 'text-orange');
        });
        if (!records.length) appendEmptyTableRow(historyBody, 6, 'Chưa có dữ liệu đo.');

        const anomalies = records.filter(record => {
            const flags = String(record.fault_flags || 'NORMAL');
            return Number(record.ai_class || 0) !== 0 || flags !== 'NORMAL';
        }).slice(-20).reverse();
        anomalyBody.replaceChildren();
        anomalies.forEach(record => {
            const flags = record.fault_flags || 'NORMAL';
            const className = AI_HISTORY_LABELS[Number(record.ai_class)] || (flags === 'NORMAL' ? 'Bất thường' : flags);
            const confidence = Number(record.ai_score);
            const aiClass = Number(record.ai_class || 0);
            const anomalyScore = Number.isFinite(confidence) && Number(record.ai_time) > 0
                ? (aiClass === 0 ? 1 - confidence / 100 : confidence / 100).toFixed(2)
                : '--';
            appendTableRow(anomalyBody, [
                formatHistoryTime(record.timestamp), className, anomalyScore,
                `${record.voltage ?? '--'} V`, `${record.current ?? '--'} A`,
                `${record.temperature ?? '--'} °C`, flags === 'NORMAL' ? 'Cảnh báo AI' : flags
            ], 'text-orange');
        });
        if (!anomalies.length) appendEmptyTableRow(anomalyBody, 7, 'Chưa ghi nhận bất thường.');
    } catch (error) {
        console.error('History loading error:', error);
    }
}

loadHistory();
setInterval(loadHistory, 15000);

// ============================================================
// 5. SYSTEM CONFIGURATION
// ============================================================
async function updateBatteryConfig() {
    const selectEl = document.getElementById('config-battery-type');
    const button = document.getElementById('config-apply');
    const message = document.getElementById('config-message');
    const batteryId = selectEl.value;
    const batteryText = selectEl.options[selectEl.selectedIndex].text;
    const cmdStr = `SET_PIN:${batteryId}`;
    
    // Parse dung lượng từ text (VD: "Generic 18650 3.0Ah" -> 3.0)
    let capacity = 2.2;
    const match = batteryText.match(/(\d+(?:\.\d+)?)\s*Ah/i);
    if(match) capacity = parseFloat(match[1]);

    button.disabled = true;
    message.innerText = 'Đang gửi cấu hình…';
    message.className = 'text-sm text-muted';
    try {
        const res = await fetch('/api/command', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ command: cmdStr, capacity: capacity })
        });
        
        if(res.ok) {
            message.innerText = `Đã đưa cấu hình ${batteryText} vào hàng đợi. Thiết bị sẽ nhận lệnh khi kết nối.`;
            message.className = 'text-sm text-green';
        } else {
            throw new Error(`HTTP ${res.status}`);
        }
    } catch (e) {
        console.error("Config error:", e);
        message.innerText = 'Không gửi được cấu hình. Hãy kiểm tra kết nối rồi thử lại.';
        message.className = 'text-sm text-red';
    } finally {
        button.disabled = false;
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
            const message = document.getElementById('reset-message');
            message.innerText = 'Đã đưa lệnh đặt lại SOH vào hàng đợi. Thiết bị sẽ nhận lệnh khi kết nối.';
            message.className = 'text-sm text-green';
        } else {
            throw new Error(`HTTP ${res.status}`);
        }
    } catch (e) {
        console.error("Reset error:", e);
        const message = document.getElementById('reset-message');
        message.innerText = 'Không gửi được lệnh. Hãy kiểm tra kết nối rồi thử lại.';
        message.className = 'text-sm text-red';
    }
}
