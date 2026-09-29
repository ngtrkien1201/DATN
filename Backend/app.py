import os
import sys
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from flask import Flask, jsonify, request, render_template, Response
from flask_cors import CORS
from database import init_db, get_latest_data, get_history, get_all_history, insert_data, save_twin_state, load_twin_state, save_pending_command, pop_pending_command
from twin_model import battery_twin_instance
import random

app = Flask(__name__)
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
        data = request.json
        if not data:
            return jsonify({"error": "No data provided"}), 400

        v = data.get('V', 0)
        i = data.get('I', 0)
        t = data.get('T', 0)
        soc = data.get('SOC', 0)
        soh = data.get('SOH', 0)
        
        # Calculate Power and simple Energy
        p = data.get('P', round(v * i, 2))
        energy = data.get('E', round(p * (1/3600), 4)) # Use E from JSON, or assume 1 sec interval
        
        status = "Discharging" if i > 0 else ("Charging" if i < 0 else "Idle")
        
        # AI Metrics from ESP32
        ai_class = data.get('AI_Class', 0)
        ai_score = data.get('AI_Score', 0.0)
        ai_time = data.get('AI_Time', 0)
        
        # Arrays from ESP32 4S
        cv = data.get('CV', [0.0, 0.0, 0.0, 0.0])
        csoc = data.get('CSOC', [0, 0, 0, 0])
        csoh = data.get('CSOH', [100, 100, 100, 100])
        cai_class = data.get('CAI_Class', [0, 0, 0, 0])
        
        # Load old state, Sync, and Save state
        _load_state()
        battery_twin_instance.sync(data)
        _save_state()
        
        twin_state = battery_twin_instance.twin_state
        
        # Insert into DB WITH Twin and AI data
        insert_data(
            v, i, p, energy, t, soc, soh, status,
            twin_ocv=twin_state.get('ocv', 0),
            twin_r0=twin_state.get('internal_resistance', 0),
            twin_vp=twin_state.get('polarization_voltage', 0),
            ai_class=ai_class,
            ai_score=ai_score,
            ai_time=ai_time,
            cv=cv,
            csoc=csoc,
            csoh=csoh,
            cai_class=cai_class
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
    
    # Giả lập trạng thái tổng quan
    health_status = "Excellent" if data['soh'] > 90 else ("Warning" if data['soh'] > 70 else "Fault")
    
    return jsonify({
        "status": "Connected",
        "battery_state": health_status,
        "voltage": data['voltage'],
        "current": data['current'],
        "power": data['power'],
        "energy": data['energy'],
        "temperature": data['temperature'],
        "soc": data['soc'],
        "soh": data['soh'],
        "charging_status": data['status'],
        "cv": data.get('cv', [0.0, 0.0, 0.0, 0.0]),
        "csoc": data.get('csoc', [0, 0, 0, 0]),
        "csoh": data.get('csoh', [100, 100, 100, 100]),
        "cai_class": data.get('cai_class', [0, 0, 0, 0])
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
    
    soh = data['soh']
    cycles = int((soh - 70) * 10) if soh > 70 else 0
    fail_prob = round((100 - soh) * 0.5, 1)
    
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
    docs = get_all_history()
    
    # Create an in-memory string buffer
    si = io.StringIO()
    writer = csv.writer(si)
    
    # Write header
    writer.writerow(['Timestamp', 'Pack_V', 'Pack_I', 'Pack_T', 'Pack_SOC', 'Pack_SOH', 'Twin_OCV', 'Twin_R0', 'Twin_Vp', 
                     'Cell1_V', 'Cell2_V', 'Cell3_V', 'Cell4_V', 
                     'Cell1_SOC', 'Cell2_SOC', 'Cell3_SOC', 'Cell4_SOC'])
    
    # Write data
    for doc in docs:
        cv = doc.get('cv', [0,0,0,0])
        csoc = doc.get('csoc', [0,0,0,0])
        writer.writerow([
            doc.get('timestamp', ''),
            doc.get('voltage', 0),
            doc.get('current', 0),
            doc.get('temperature', 0),
            doc.get('soc', 0),
            doc.get('soh', 0),
            doc.get('twin_ocv', 0),
            doc.get('twin_r0', 0),
            doc.get('twin_vp', 0),
            cv[0] if len(cv) > 0 else 0,
            cv[1] if len(cv) > 1 else 0,
            cv[2] if len(cv) > 2 else 0,
            cv[3] if len(cv) > 3 else 0,
            csoc[0] if len(csoc) > 0 else 0,
            csoc[1] if len(csoc) > 1 else 0,
            csoc[2] if len(csoc) > 2 else 0,
            csoc[3] if len(csoc) > 3 else 0
        ])
    
    output = si.getvalue()
    return Response(
        output,
        mimetype="text/csv",
        headers={"Content-disposition": "attachment; filename=hybrid_dataset.csv"}
    )

@app.route('/api/clear-telemetry', methods=['GET'])
def clear_telemetry():
    from database import clear_all_history
    clear_all_history()
    return "<h1>Database Cleared Successfully!</h1><p>You can now close this tab and collect new 4S data.</p>"

@app.route('/api/history', methods=['GET'])
def get_full_history():
    history = get_history(100) 
    return jsonify(history)

@app.route('/api/command', methods=['POST'])
def send_cmd():
    cmd = request.json.get('command')
    capacity = request.json.get('capacity')
    
    if cmd and cmd.startswith('SET_PIN:'):
        # Lưu vào DB để chờ ESP32 kéo về
        save_pending_command(cmd)
        
        # Cập nhật ngay lập tức Digital Twin trên backend
        if capacity:
            try:
                cap = float(capacity)
                _load_state()
                battery_twin_instance.capacity_Ah = cap
                battery_twin_instance.capacity_As = cap * 3600
                soc = battery_twin_instance.internal_twin_soc
                if soc is None:
                    latest = get_latest_data()
                    soc = latest.get('soc', 100.0) if latest else 100.0
                    battery_twin_instance.internal_twin_soc = soc
                battery_twin_instance.remaining_capacity = round(cap * (soc / 100.0), 3)
                _save_state()
            except Exception:
                pass

    return jsonify({"status": "success", "command": cmd}), 200

@app.route('/api/simulate', methods=['POST'])
def simulate_scenario():
    req = request.json
    current = req.get("current", 0.0)
    duration = req.get("duration", 30)
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
        
        # Try a simple DB operation
        from database import db
        count = db['battery_data'].count_documents({})
        
        return jsonify({
            "status": "OK",
            "mongo_uri_set": mongo_uri != 'NOT SET',
            "mongo_uri_preview": masked,
            "record_count": count,
            "python_path": sys.path
        }), 200
    except Exception as e:
        return jsonify({"status": "ERROR", "error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True, use_reloader=False)
