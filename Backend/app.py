from flask import Flask, jsonify, request, render_template
from flask_cors import CORS
from database import init_db, get_latest_data, get_history, insert_data, save_twin_state, load_twin_state
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
    data = request.json
    if not data:
        return jsonify({"error": "No data provided"}), 400

    v = data.get('V', 0)
    i = data.get('I', 0)
    t = data.get('T', 0)
    soc = data.get('SOC', 0)
    soh = data.get('SOH', 0)
    
    # Calculate Power and simple Energy
    p = round(v * i, 2)
    energy = round(p * (1/3600), 4) # Assuming 1 sec interval
    
    status = "Charging" if i > 0 else ("Discharging" if i < 0 else "Idle")
    
    # Insert into DB
    insert_data(v, i, p, energy, t, soc, soh, status)
    
    # Load old state, Sync, and Save state
    _load_state()
    battery_twin_instance.sync(data)
    _save_state()
    
    return jsonify({"message": "Data received successfully"}), 200

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
        "charging_status": data['status']
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

@app.route('/api/history', methods=['GET'])
def get_full_history():
    history = get_history(100) 
    return jsonify(history)

@app.route('/api/command', methods=['POST'])
def send_cmd():
    # TODO: Khi không dùng MQTT, việc điều khiển ngược lại ESP32 sẽ phải dùng cơ chế Polling (ESP32 pull)
    # Tạm thời endpoint này giữ lại nhưng chưa có cơ chế đẩy
    return jsonify({"error": "MQTT disabled on Vercel. ESP32 must pull for commands."}), 501

@app.route('/api/simulate', methods=['POST'])
def simulate_scenario():
    req = request.json
    current = req.get("current", 0.0)
    duration = req.get("duration", 30)
    _load_state()
    res = battery_twin_instance.simulate_what_if(current, duration)
    return jsonify(res)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True, use_reloader=False)
