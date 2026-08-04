// ============================================================
// 1. ROUTER VIRTUAL (SPA Navigation)
// ============================================================
const menuItems = document.querySelectorAll('.menu-item');
const pages = document.querySelectorAll('.page-content');

menuItems.forEach(item => {
    item.addEventListener('click', () => {
        menuItems.forEach(i => i.classList.remove('active'));
        item.classList.add('active');
        
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
        }
    });
});

// ============================================================
// 2. SCENARIO SIMULATION (WHAT-IF)
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
    } catch (e) { alert("Error running simulation!"); }
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
        opt.plugins.legend = { display: true, labels: { color: '#64748b', font: { size: 11 } } };
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
        { label: 'Voltage', data: [], borderColor: '#3b82f6', tension: 0.4, pointRadius: 0, borderWidth: 2, yAxisID: 'y' }
    ]);
    
    // Live Monitoring
    charts.lmV = createChart('c-lm-v', [{ data: [], borderColor: '#3b82f6', backgroundColor: '#3b82f622', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.lmI = createChart('c-lm-i', [{ data: [], borderColor: '#f59e0b', backgroundColor: '#f59e0b22', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.lmT = createChart('c-lm-t', [{ data: [], borderColor: '#ef4444', backgroundColor: '#ef444422', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    charts.lmP = createChart('c-lm-p', [{ data: [], borderColor: '#10b981', backgroundColor: '#10b98122', fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }]);
    
    // Validation
    charts.valVolt = createChart('c-val-volt', [
        { label: 'Measured V', data: [], borderColor: '#3b82f6', tension: 0.4, pointRadius: 0, borderWidth: 2 },
        { label: 'Twin V', data: [], borderColor: '#10b981', tension: 0.4, pointRadius: 0, borderWidth: 2 }
    ], true);
    
    charts.valSoc = createChart('c-val-soc', [
        { label: 'Est. SOC', data: [], borderColor: '#3b82f6', tension: 0.4, pointRadius: 0, borderWidth: 2 },
        { label: 'Twin SOC', data: [], borderColor: '#10b981', tension: 0.4, pointRadius: 0, borderWidth: 2 }
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

// ============================================================
// 4. DATA POLLING & UI UPDATE
// ============================================================
async function updateTwin() {
    try {
        const res = await fetch(`/api/digital-twin`);
        if (!res.ok) return;
        const d = await res.json();
        
        const now = new Date();
        const timeStr = now.toLocaleTimeString();

        // ------------------ GLOBAL STATUS BAR ------------------
        document.getElementById('gb-time').innerText = timeStr;
        const isOnline = d.sync_metrics.status !== "Disconnected";
        document.getElementById('gb-sys').innerText = isOnline ? "Online" : "Offline";
        document.getElementById('gb-sys').className = isOnline ? "text-green" : "text-red";
        
        document.getElementById('gb-stm').innerText = isOnline ? "Connected" : "Disconnected";
        document.getElementById('gb-stm').className = isOnline ? "" : "text-red";
        document.getElementById('gb-esp').innerText = isOnline ? "Connected" : "Disconnected";
        document.getElementById('gb-esp').className = isOnline ? "" : "text-red";
        document.getElementById('gb-mqtt').innerText = isOnline ? "Connected" : "Disconnected";
        document.getElementById('gb-mqtt').className = isOnline ? "" : "text-red";
        
        document.getElementById('gb-mode').innerText = d.real.status;
        
        // ------------------ PAGE 1: SYSTEM OVERVIEW ------------------
        document.getElementById('ov-v').innerText = d.real.voltage + ' V';
        document.getElementById('ov-i').innerText = d.real.current + ' A';
        document.getElementById('ov-t').innerText = d.real.temperature + ' °C';
        document.getElementById('ov-p').innerText = (d.real.power || (d.real.voltage * d.real.current).toFixed(3)) + ' W';
        document.getElementById('ov-e').innerText = (d.real.energy || 0.0) + ' Wh';
        
        document.getElementById('ov-soc').innerText = d.real.soc + ' %';
        document.getElementById('ov-soh').innerText = d.real.soh + ' %';
        document.getElementById('ov-cycle').innerText = d.twin.cycle_count;
        
        document.getElementById('ov-tv').innerText = d.twin.terminal_voltage + ' V';
        document.getElementById('ov-tsoc').innerText = d.twin.soc + ' %';
        document.getElementById('ov-err').innerText = (d.errors.voltage_mv !== undefined ? d.errors.voltage_mv : (d.errors.voltage * 1000).toFixed(1)) + ' mV';
        document.getElementById('ov-sync').innerText = d.sync_metrics.status;
        
        document.getElementById('ov-anomaly').innerText = d.edge_ai ? d.edge_ai.anomaly_class : "Normal";
        document.getElementById('ov-ascore').innerText = d.edge_ai ? d.edge_ai.anomaly_score : "0.05";
        
        // ------------------ PAGE 2: LIVE MONITORING ------------------
        document.getElementById('lm-v').innerHTML = `${d.real.voltage} <small>V</small>`;
        document.getElementById('lm-i').innerHTML = `${d.real.current} <small>A</small>`;
        document.getElementById('lm-p').innerHTML = `${d.real.power || (d.real.voltage * d.real.current).toFixed(3)} <small>W</small>`;
        document.getElementById('lm-soc').innerHTML = `${d.real.soc} <small>%</small>`;
        
        const liveTb = document.getElementById('live-table-body');
        if(liveTb) {
            const row = document.createElement('tr');
            row.innerHTML = `<td>${timeStr}</td><td>${d.real.voltage} V</td><td>${d.real.current} A</td><td>${d.real.temperature} °C</td><td>${d.real.power || 0} W</td><td>${d.real.energy || 0} Wh</td><td class="text-green">${d.real.soc} %</td>`;
            liveTb.insertBefore(row, liveTb.firstChild);
            if(liveTb.children.length > 50) liveTb.removeChild(liveTb.lastChild);
        }
        
        // ------------------ PAGE 3: DIGITAL TWIN ------------------
        document.getElementById('dt-pb-v').innerText = d.real.voltage + ' V';
        document.getElementById('dt-pb-i').innerText = d.real.current + ' A';
        document.getElementById('dt-pb-soc').innerText = d.real.soc + ' %';
        document.getElementById('dt-pb-fill').style.height = `${d.real.soc}%`;
        
        document.getElementById('dt-sync-status').innerText = d.sync_metrics.status;
        document.getElementById('dt-sync-pv').innerText = d.real.voltage + ' V';
        document.getElementById('dt-sync-tv').innerText = d.twin.terminal_voltage + ' V';
        
        let residual = Math.abs(d.real.voltage - d.twin.terminal_voltage).toFixed(3);
        document.getElementById('dt-sync-res').innerText = residual + ' V';
        document.getElementById('dt-sync-err').innerText = d.errors.voltage + ' %';
        document.getElementById('dt-sync-age').innerText = (d.sync_metrics.latency_ms / 1000).toFixed(1) + ' s';
        
        document.getElementById('dt-tb-ocv').innerText = d.twin.ocv + ' V';
        document.getElementById('dt-tb-tv').innerText = d.twin.terminal_voltage + ' V';
        document.getElementById('dt-tb-soc').innerText = d.twin.soc + ' %';
        document.getElementById('dt-tb-fill').style.height = `${d.twin.soc}%`;
        
        document.getElementById('dt-p-r0').innerText = d.model_parameters.R0_mOhm + ' mΩ';
        document.getElementById('dt-p-r1').innerText = (d.model_parameters.R1 * 1000).toFixed(1) + ' mΩ';
        document.getElementById('dt-p-c1').innerText = d.model_parameters.C1 + ' F';
        document.getElementById('dt-p-vp').innerText = d.twin.polarization_voltage + ' V';
        
        // ------------------ PAGE 4: EDGE AI ------------------
        if(d.edge_ai) {
            document.getElementById('ai-score').innerText = d.edge_ai.anomaly_score;
            document.getElementById('ai-status').innerText = d.edge_ai.status;
            document.getElementById('ai-class').innerText = d.edge_ai.anomaly_class;
            document.getElementById('ai-conf').innerText = d.edge_ai.confidence + '%';
            document.getElementById('ai-time').innerText = d.edge_ai.inference_time + ' ms';
        }
        
        // ------------------ PAGE 5: VALIDATION ------------------
        if(d.validation) {
            document.getElementById('val-v-mae').innerHTML = `${d.validation.v_mae} <small>V</small>`;
            document.getElementById('val-v-rmse').innerHTML = `${d.validation.v_rmse} <small>V</small>`;
            document.getElementById('val-v-max').innerHTML = `${d.validation.v_max_err} <small>V</small>`;
            document.getElementById('val-soc-mae').innerHTML = `${d.validation.soc_mae} <small>%</small>`;
        }
        
        // ------------------ PAGE 6: RUL ------------------
        document.getElementById('rul-cycles').innerText = d.aging_model.rul_cycles.split(' ')[0];
        document.getElementById('rul-conf').innerText = d.aging_model.rul_confidence_range || 'N/A';
        document.getElementById('rul-soh-cap').innerText = d.twin.soh + ' %'; // simplification
        document.getElementById('rul-soh-res').innerText = (100 - parseFloat(d.aging_model.resistance_growth)).toFixed(1) + ' %';
        document.getElementById('rul-fade').innerText = d.aging_model.capacity_fade;
        document.getElementById('rul-growth').innerText = d.aging_model.resistance_growth;
        document.getElementById('rul-est-cap').innerText = d.twin.remaining_capacity + ' Ah';
        document.getElementById('rul-stage').innerText = d.aging_model.aging_stage;

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
        
        for (let key in charts) {
            if(charts[key]) charts[key].update('none');
        }

        lucide.createIcons();
    } catch (e) {
        console.error("Fetch error: ", e);
    }
}

setInterval(updateTwin, 1000);
updateTwin();

// ============================================================
// 5. SYSTEM CONFIGURATION
// ============================================================
async function updateBatteryConfig() {
    const capacity = document.getElementById('config-battery-type').value;
    const cmdStr = `SET_BATTERY:${capacity}`;
    
    try {
        const res = await fetch('/api/command', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ command: cmdStr })
        });
        
        if(res.ok) {
            alert(`Configuration sent! Battery capacity set to ${capacity}Ah.`);
        } else {
            alert("Failed to send configuration.");
        }
    } catch (e) {
        console.error("Config error:", e);
        alert("Error sending configuration.");
    }
}
