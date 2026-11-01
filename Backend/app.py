import os
import sys
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from flask import Flask, jsonify, request, render_template, Response
from flask_cors import CORS
from database import (
    init_db, get_latest_data, get_history, get_all_history, 
    insert_data, save_twin_state, load_twin_state, 
    save_pending_command, pop_pending_command,
    clear_all_history, _use_mongo, _in_memory_history, db
)
from twin_model import battery_twin_instance
import random

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
app = Flask(
    __name__,
    template_folder=os.path.join(BASE_DIR, 'templates'),
    static_folder=os.path.join(BASE_DIR, 'static')
)
CORS(app) # Cho phép React/Web gọi API

# Khởi tạo DB
init_db()

def _load_state():
    """ Load Digital Twin state from DB for Serverless environment """
    state = load_twin_state()
    if state:
        battery_twin_instance.from_dict(state)

def _save_state():
    """ Save Digital Twin state to DB """
    save_twin_state(battery_twin_instance.to_dict())

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/telemetry', methods=['POST'])
def receive_telemetry():
    """ Nhận dữ liệu từ ESP32 qua HTTP POST (Thay thế cho MQTT) """
    try:
        data = request.get_json(silent=True)
        if not data or not isinstance(data, dict):
            return jsonify({"error": "No valid JSON data provided"}), 400

        v = float(data.get('V') or 0.0)
        i = float(data.get('I') or 0.0)
        t = float(data.get('T') or 0.0)
        soc = float(data.get('SOC') or 0.0)
        soh = float(data.get('SOH') or 100.0)
        
        # Calculate Power and simple Energy
        p = float(data.get('P', round(v * i, 3)))
        energy = float(data.get('E', round(p * (1/3600), 4)))
        
        status = data.get('Status') or ("Discharging" if i > 0.01 else ("Charging" if i < -0.01 else "Idle"))
        
        # AI Metrics from ESP32
        ai_class = int(data.get('AI_Class') or 0)
        ai_score = float(data.get('AI_Score') or 0.0)
        ai_time = int(data.get('AI_Time') or 0)
        fault_flags = str(data.get('Fault_Flags') or 'NORMAL')
        
        # Arrays from ESP32 4S
        cv_raw = data.get('CV', [0.0, 0.0, 0.0, 0.0])
        cv = [float(x or 0.0) for x in cv_raw] if isinstance(cv_raw, list) else [0.0, 0.0, 0.0, 0.0]
        while len(cv) < 4: cv.append(0.0)
        
        csoc_raw = data.get('CSOC', [0, 0, 0, 0])
        csoc = [int(x or 0) for x in csoc_raw] if isinstance(csoc_raw, list) else [0, 0, 0, 0]
        while len(csoc) < 4: csoc.append(0)

        csoh_raw = data.get('CSOH', [100, 100, 100, 100])
        csoh = [int(x or 100) for x in csoh_raw] if isinstance(csoh_raw, list) else [100, 100, 100, 100]
        while len(csoh) < 4: csoh.append(100)

        cai_class_raw = data.get('CAI_Class', [0, 0, 0, 0])
        cai_class = [int(x or 0) for x in cai_class_raw] if isinstance(cai_class_raw, list) else [0, 0, 0, 0]
        while len(cai_class) < 4: cai_class.append(0)
        
        # Load old state, Sync, and Save state
        _load_state()
        battery_twin_instance.sync(data)
        _save_state()
        
        twin_state = battery_twin_instance.twin_state
        
        # Insert into DB WITH Twin and AI data
        insert_data(
            v, i, p, energy, t, soc, soh, status,
            twin_ocv=twin_state.get('ocv', 0.0),
            twin_r0=twin_state.get('internal_resistance', 0.0),
            twin_vp=twin_state.get('polarization_voltage', 0.0),
            ai_class=ai_class,
            ai_score=ai_score,
            ai_time=ai_time,
            cv=cv,
            csoc=csoc,
            csoh=csoh,
            cai_class=cai_class,
            fault_flags=fault_flags
        )
        
        # Kiểm tra xem có lệnh chờ nào không để gửi về cho ESP32
        pending_cmd = pop_pending_command()
        response = {"message": "Data received successfully"}
        if pending_cmd:
            response["command"] = pending_cmd
        
        return jsonify(response), 200
    except Exception as e:
        import traceback
        err_msg = traceback.format_exc()
        return jsonify({"error": str(e), "traceback": err_msg}), 500

@app.route('/api/dashboard', methods=['GET'])
def get_dashboard():
    data = get_latest_data()
    if not data:
        return jsonify({"error": "No data yet"}), 404
    
    soh = float(data.get('soh') or 100.0)
    health_status = "Excellent" if soh > 90 else ("Warning" if soh > 70 else "Fault")
    
    return jsonify({
        "status": "Connected",
        "battery_state": health_status,
        "voltage": data.get('voltage', 0.0),
        "current": data.get('current', 0.0),
        "power": data.get('power', 0.0),
        "energy": data.get('energy', 0.0),
        "temperature": data.get('temperature', 0.0),
        "soc": data.get('soc', 0.0),
        "soh": soh,
        "charging_status": data.get('status', 'Idle'),
        "cv": data.get('cv', [0.0, 0.0, 0.0, 0.0]),
        "csoc": data.get('csoc', [0, 0, 0, 0]),
        "csoh": data.get('csoh', [100, 100, 100, 100]),
        "cai_class": data.get('cai_class', [0, 0, 0, 0]),
        "ai_class": data.get('ai_class', 0),
        "fault_flags": data.get('fault_flags', 'NORMAL')
    })

@app.route('/api/monitor', methods=['GET'])
def get_monitor():
    history = get_history(30)
    return jsonify(history)

@app.route('/api/digital-twin', methods=['GET'])
def get_twin():
    _load_state()
    return jsonify(battery_twin_instance.get_state())

@app.route('/api/edge-ai', methods=['GET'])
def get_edge_ai():
    data = get_latest_data()
    if not data:
        return jsonify({"error": "No data yet"}), 404
    
    soh = float(data.get('soh') or 100.0)
    cycles = int((soh - 70) * 10) if soh > 70 else 0
    fail_prob = round((100.0 - soh) * 0.5, 1)
    
    rec = "Normal Operation"
    if fail_prob > 10: rec = "Schedule Maintenance"
    if fail_prob > 30: rec = "Replace Battery Immediately"
    
    return jsonify({
        "health_score": soh,
        "remaining_useful_life": f"{cycles} cycles",
        "failure_probability": f"{fail_prob}%",
        "recommendation": rec
    })

@app.route('/api/export-csv', methods=['GET'])
def export_csv():
    import io
    import csv
    docs = get_all_history(limit=5000)

    si = io.StringIO()
    writer = csv.writer(si)

    # Header — bao gồm cột Label (Fault_Type) và Fault_Flags (Tất cả lỗi)
    writer.writerow([
        'Timestamp',
        'Pack_V', 'Pack_I', 'Pack_T', 'Pack_SOC', 'Pack_SOH',
        'Twin_OCV', 'Twin_R0', 'Twin_Vp',
        'Cell1_V', 'Cell2_V', 'Cell3_V', 'Cell4_V',
        'Cell1_SOC', 'Cell2_SOC', 'Cell3_SOC', 'Cell4_SOC',
        'Label',        # ← Nhãn chính để train AI theo Thứ tự ưu tiên (Priority 1->5)
        'Fault_Flags'   # ← Danh sách toàn bộ lỗi đồng thời (OV, UV, OC, OT, CI, NORMAL)
    ])

    for doc in docs:
        cv_raw = doc.get('cv') or [0.0, 0.0, 0.0, 0.0]
        cv = [float(x or 0.0) for x in cv_raw] if isinstance(cv_raw, list) else [0.0, 0.0, 0.0, 0.0]
        while len(cv) < 4: cv.append(0.0)

        csoc_raw = doc.get('csoc') or [0, 0, 0, 0]
        csoc = [int(x or 0) for x in csoc_raw] if isinstance(csoc_raw, list) else [0, 0, 0, 0]
        while len(csoc) < 4: csoc.append(0)

        max_v    = max(cv) if cv else 0.0
        min_v    = min(cv) if cv else 0.0
        delta_v  = max_v - min_v if cv else 0.0
        curr     = abs(float(doc.get('current') or 0.0))
        temp     = float(doc.get('temperature') or 0.0)
        pack_v   = float(doc.get('voltage') or 0.0)
        pack_soc = float(doc.get('soc') or 0.0)

        # 1. Thu thập toàn bộ cờ lỗi đang xảy ra đồng thời (Fault_Flags)
        flags = []
        if max_v > 4.20:
            flags.append('OV')
        if min_v < 3.00 and pack_v > 1.0:
            flags.append('UV')
        if curr > 2.0:
            flags.append('OC')
        if temp > 50.0:
            flags.append('OT')
        if delta_v > 0.20 and pack_soc > 5 and pack_v > 1.0:
            flags.append('CI')

        active_fault_flags = ",".join(flags) if flags else "NORMAL"
        doc_flags = doc.get('fault_flags') or active_fault_flags

        # 2. Phân xử nhãn duy nhất cho AI (Label / Fault_Type) theo THỨ TỰ ƯU TIÊN:
        # Priority 1: Over-voltage (OV)
        # Priority 2: Under-voltage (UV)
        # Priority 3: Over-current (OC)
        # Priority 4: Over-temperature (OT)
        # Priority 5: Cell-Imbalance (CI)
        # Priority 6: Normal
        if doc.get('label'):
            label = doc.get('label')
        elif 'OV' in flags or 'OV' in doc_flags:
            label = 'Over-voltage'
        elif 'UV' in flags or 'UV' in doc_flags:
            label = 'Under-voltage'
        elif 'OC' in flags or 'OC' in doc_flags:
            label = 'Over-current'
        elif 'OT' in flags or 'OT' in doc_flags:
            label = 'Over-temperature'
        elif 'CI' in flags or 'CI' in doc_flags:
            label = 'Cell-Imbalance'
        else:
            label = 'Normal'

        writer.writerow([
            doc.get('timestamp', ''),
            pack_v,
            float(doc.get('current') or 0.0),
            temp,
            pack_soc,
            float(doc.get('soh') or 0.0),
            float(doc.get('twin_ocv') or 0.0),
            float(doc.get('twin_r0') or 0.0),
            float(doc.get('twin_vp') or 0.0),
            cv[0], cv[1], cv[2], cv[3],
            csoc[0], csoc[1], csoc[2], csoc[3],
            label,
            doc_flags
        ])

    output = si.getvalue()
    return Response(
        output,
        mimetype="text/csv",
        headers={"Content-disposition": "attachment; filename=battery_dataset_4s.csv"}
    )

@app.route('/api/clear-telemetry', methods=['GET', 'POST'])
def clear_telemetry():
    """Xóa dữ liệu (Hỗ trợ POST để an toàn và GET với nút xác nhận)"""
    if request.method == 'POST' or request.args.get('confirm') == 'yes':
        clear_all_history()
        return "<h1>Database Cleared Successfully!</h1><p>Dữ liệu lịch sử đã được xóa sạch. Bạn có thể bắt đầu thu thập dữ liệu mới.</p>"
    
    return """
    <div style="font-family: Arial, sans-serif; padding: 30px; text-align: center;">
        <h2>XÁC NHẬN XÓA TOÀN BỘ CƠ SỞ DỮ LIỆU TELEMETRY?</h2>
        <p style="color: red;">Cảnh báo: Hành động này không thể hoàn tác.</p>
        <form method="POST">
            <button type="submit" style="background: #e53e3e; color: white; border: none; padding: 12px 24px; font-size: 16px; border-radius: 6px; cursor: pointer;">
                Xác nhận Xóa Ngay
            </button>
            <a href="/" style="margin-left: 15px; color: #4a5568;">Hủy bỏ</a>
        </form>
    </div>
    """

@app.route('/api/history', methods=['GET'])
def get_full_history():
    history = get_history(100) 
    return jsonify(history)

@app.route('/api/command', methods=['POST'])
def send_cmd():
    req_json = request.get_json(silent=True) or {}
    cmd = req_json.get('command')
    capacity = req_json.get('capacity')
    
    if cmd:
        # Lưu vào hàng đợi để chờ ESP32 kéo về qua HTTP POST telemetry
        save_pending_command(str(cmd))
        
        # Cập nhật ngay lập tức Digital Twin trên backend nếu có cấu hình dung lượng
        if cmd.startswith('SET_PIN:') and capacity:
            try:
                cap = float(capacity)
                _load_state()
                battery_twin_instance.capacity_Ah = cap
                battery_twin_instance.capacity_As = cap * 3600
                soc = battery_twin_instance.internal_twin_soc
                if soc is None:
                    latest = get_latest_data()
                    soc = float(latest.get('soc', 100.0) if latest else 100.0)
                    battery_twin_instance.internal_twin_soc = soc
                battery_twin_instance.remaining_capacity = round(cap * (soc / 100.0), 3)
                _save_state()
            except Exception:
                pass

    return jsonify({"status": "success", "command": cmd}), 200

@app.route('/api/simulate', methods=['POST'])
def simulate_scenario():
    req = request.get_json(silent=True) or {}
    try:
        current = float(req.get("current", 0.0) or 0.0)
    except (TypeError, ValueError):
        current = 0.0

    try:
        duration = float(req.get("duration", 30) or 30)
    except (TypeError, ValueError):
        duration = 30.0

    _load_state()
    res = battery_twin_instance.simulate_what_if(current, duration)
    return jsonify(res)

@app.route('/api/reset_soh', methods=['POST'])
def reset_soh():
    """ Endpoint để gửi lệnh RESET_SOH tới phần cứng """
    try:
        save_pending_command("RESET_SOH")
        return jsonify({"status": "success", "message": "Command RESET_SOH queued for ESP32"}), 200
    except Exception as e:
        return jsonify({"status": "error", "error": str(e)}), 500

@app.route('/api/test', methods=['GET'])
def test_connection():
    """Test endpoint to diagnose Vercel/MongoDB connection"""
    try:
        import os
        mongo_uri = os.environ.get('MONGO_URI', 'NOT SET')
        # Mask password for security
        masked = mongo_uri[:20] + '...' if len(mongo_uri) > 20 else mongo_uri
        
        count = 0
        if _use_mongo and db is not None:
            try:
                count = db['battery_data'].count_documents({})
            except Exception as ex:
                count = f"Mongo error: {ex}"
        else:
            count = len(_in_memory_history)
        
        return jsonify({
            "status": "OK",
            "mongo_connected": _use_mongo,
            "mongo_uri_set": mongo_uri != 'NOT SET',
            "mongo_uri_preview": masked,
            "record_count": count,
            "python_path": sys.path
        }), 200
    except Exception as e:
        return jsonify({"status": "ERROR", "error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True, use_reloader=False)
