// ============================================================
// 1. ROUTER VIRTUAL (Chuyển trang SPA)
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
        
        const targetPage = document.getElementById(item.getAttribute('data-target'));
        targetPage.classList.remove('hidden');
        
        // Trigger reflow to restart animation
        void targetPage.offsetWidth; 
        targetPage.style.animation = 'fadeUp 0.5s cubic-bezier(0.16, 1, 0.3, 1) forwards';
    });
});

// ============================================================
// 2. SCENARIO SIMULATION (WHAT-IF)
// ============================================================
async function runWhatIf() {
    const current = document.getElementById('sim-current').value;
    const duration = document.getElementById('sim-duration').value;
    
    try {
        const res = await fetch(`http://${window.location.hostname}:5000/api/simulate`, { 
            method: 'POST', 
            headers: {'Content-Type': 'application/json'}, 
            body: JSON.stringify({current: parseFloat(current), duration: parseInt(duration)}) 
        });
        const data = await res.json();
        
        document.getElementById('sim-results').style.display = 'block';
        document.getElementById('sim-r-v').innerText = data.sim_voltage + ' V';
        document.getElementById('sim-r-soc').innerText = data.sim_soc + ' %';
        document.getElementById('sim-r-t').innerText = data.sim_temp + ' °C';
        document.getElementById('sim-r-vp').innerText = data.sim_vp + ' V';
        
    } catch (e) { alert("Lỗi khi chạy Sandbox Simulation!"); }
}

// ============================================================
// 3. KHỞI TẠO BIỂU ĐỒ (Chart.js) - Tái sử dụng code cũ
// ============================================================
let charts = {};

function createDualChart(canvasId, label1, color1, label2, color2) {
    const el = document.getElementById(canvasId);
    if (!el) return null;
    return new Chart(el.getContext('2d'), {
        type: 'line',
        data: { labels: [], datasets: [
            { label: label1, data: [], borderColor: color1, tension: 0.4, pointRadius: 0, borderWidth: 2 },
            { label: label2, data: [], borderColor: color2, tension: 0.4, pointRadius: 0, borderWidth: 2 }
        ]},
        options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { labels: { color: '#94a3b8', font: { size: 11 } } } }, scales: { x: { display: false }, y: { grid: { color: 'rgba(255,255,255,0.03)' }, ticks: { color: '#94a3b8', font: { size: 10 } } } } }
    });
}

function createSingleChart(canvasId, color) {
    const el = document.getElementById(canvasId);
    if (!el) return null;
    return new Chart(el.getContext('2d'), {
        type: 'line',
        data: { labels: [], datasets: [{ data: [], borderColor: color, backgroundColor: `${color}22`, fill: true, tension: 0.4, pointRadius: 0, borderWidth: 2 }] },
        options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { x: { display: false }, y: { grid: { color: 'rgba(255,255,255,0.03)' }, ticks: { color: '#94a3b8', font: { size: 10 } } } } }
    });
}

function initCharts() {
    charts.volt = createDualChart('c-volt', 'Real Voltage', '#3b82f6', 'Twin Voltage', '#10b981');
    charts.soc = createDualChart('c-soc', 'Real SOC', '#3b82f6', 'Twin SOC', '#10b981');
    charts.err = createSingleChart('c-err', '#ef4444');
    charts.res = createSingleChart('c-res', '#f59e0b');

    charts.realV = createSingleChart('c-real-v', '#3b82f6');
    charts.realI = createSingleChart('c-real-i', '#f59e0b');
    charts.realT = createSingleChart('c-real-t', '#ef4444');
    charts.realSOC = createSingleChart('c-real-soc', '#10b981');
}
initCharts();

const MAX_POINTS = 60;
const buf = { labels: [], realV: [], twinV: [], realSOC: [], twinSOC: [], errV: [], resH: [], realI: [], realT: [] };
function pushBuf(key, val) { buf[key].push(val); if (buf[key].length > MAX_POINTS) buf[key].shift(); }

// ============================================================
// 4. CẬP NHẬT DỮ LIỆU TWIN
// ============================================================
async function updateTwin() {
    try {
        const res = await fetch(`http://${window.location.hostname}:5000/api/digital-twin`);
        if (!res.ok) return;
        const d = await res.json();

        // Sub-modules Status
        document.getElementById('st-ecm').innerText = d.sub_modules.ecm_solver;
        document.getElementById('st-soc').innerText = d.sub_modules.soc_estimator;
        document.getElementById('st-soh').innerText = d.sub_modules.soh_estimator;
        document.getElementById('st-pred').innerText = d.sub_modules.prediction_engine;

        // Diagnostics
        document.getElementById('diag-iter').innerText = d.diagnostics.model_iteration;
        document.getElementById('diag-res').innerText = d.diagnostics.residual.toFixed(5);
        document.getElementById('diag-acc').innerText = d.diagnostics.ecm_accuracy;
        document.getElementById('diag-conf').innerText = d.diagnostics.prediction_confidence;

        // Physical
        document.getElementById('pb-v').innerText = d.real.voltage + ' V';
        document.getElementById('pb-i').innerText = d.real.current + ' A';
        document.getElementById('pb-soc').innerText = d.real.soc + ' %';
        document.getElementById('pb-visual-fill').style.height = `${d.real.soc}%`;
        document.getElementById('pb-soh2').innerText = d.real.soh + ' %'; // Page 5

        // Twin
        document.getElementById('db-ocv').innerText = d.twin.ocv + ' V';
        document.getElementById('db-tv').innerText = d.twin.terminal_voltage + ' V';
        document.getElementById('db-pv').innerText = d.twin.polarization_voltage + ' V';
        document.getElementById('db-soc').innerText = d.twin.soc + ' %';
        document.getElementById('db-visual-fill').style.height = `${d.twin.soc}%`;
        document.getElementById('db-soh2').innerText = d.twin.soh + ' %'; // Page 5

        // Parameters
        document.getElementById('p-r0').innerText = d.model_parameters.R0 + ' Ω';
        document.getElementById('p-r1').innerText = d.model_parameters.R1 + ' Ω';
        document.getElementById('p-c1').innerText = d.model_parameters.C1 + ' F';

        // Aging & RUL
        document.getElementById('rul-cycles').innerText = d.aging_model.rul_cycles.split(' ')[0];
        document.getElementById('rul-months').innerText = '~ ' + d.aging_model.rul_months;
        document.getElementById('ag-cap').innerText = d.aging_model.capacity_fade;
        document.getElementById('ag-res').innerText = d.aging_model.resistance_growth;
        document.getElementById('ag-stage').innerText = d.aging_model.aging_stage;

        // Multi-Horizon Prediction
        let html = '';
        d.predictions.forEach(p => {
            html += `<tr><td style="padding:8px 0;">+ ${p.horizon}</td><td class="text-blue">${p.voltage} V</td><td class="text-green">${p.soc} %</td><td>${p.capacity} Ah</td></tr>`;
        });
        document.getElementById('multi-pred-body').innerHTML = html;

        // Biểu đồ
        pushBuf('labels', d.sync_metrics.last_update);
        pushBuf('realV', d.real.voltage);
        pushBuf('realI', d.real.current);
        pushBuf('realT', d.real.temperature);
        pushBuf('twinV', d.twin.terminal_voltage);
        pushBuf('realSOC', d.real.soc);
        pushBuf('twinSOC', d.twin.soc);
        pushBuf('errV', d.errors.voltage);
        pushBuf('resH', d.twin.internal_resistance);

        if(charts.volt) {
            charts.volt.data.labels = buf.labels; charts.volt.data.datasets[0].data = buf.realV; charts.volt.data.datasets[1].data = buf.twinV; charts.volt.update('none');
            charts.soc.data.labels = buf.labels; charts.soc.data.datasets[0].data = buf.realSOC; charts.soc.data.datasets[1].data = buf.twinSOC; charts.soc.update('none');
            charts.err.data.labels = buf.labels; charts.err.data.datasets[0].data = buf.errV; charts.err.update('none');
            charts.res.data.labels = buf.labels; charts.res.data.datasets[0].data = buf.resH; charts.res.update('none');
        }
        if(charts.realV) {
            charts.realV.data.labels = buf.labels; charts.realV.data.datasets[0].data = buf.realV; charts.realV.update('none');
            charts.realI.data.labels = buf.labels; charts.realI.data.datasets[0].data = buf.realI; charts.realI.update('none');
            charts.realT.data.labels = buf.labels; charts.realT.data.datasets[0].data = buf.realT; charts.realT.update('none');
            charts.realSOC.data.labels = buf.labels; charts.realSOC.data.datasets[0].data = buf.realSOC; charts.realSOC.update('none');
        }
        
        // History Table
        const tb = document.getElementById('history-tbody');
        if(tb) {
            const row = document.createElement('tr');
            row.style.borderBottom = '1px solid rgba(255,255,255,0.05)';
            row.innerHTML = `<td>${d.sync_metrics.last_update}</td><td>${d.real.voltage}</td><td>${d.real.current}</td><td>${d.real.temperature}</td><td>${d.real.soc}</td><td>${d.real.status}</td>`;
            tb.insertBefore(row, tb.firstChild);
            if(tb.children.length > 50) tb.removeChild(tb.lastChild);
        }

        lucide.createIcons();
    } catch (e) {}
}

setInterval(updateTwin, 1000);
updateTwin();
