"""
MQTT Worker (Optional Standalone Daemon)
Lưu ý: Hệ thống hiện tại sử dụng HTTP POST trực tiếp từ ESP32 lên Backend Vercel Serverless.
Module này được giữ lại như một tùy chọn dự phòng khi triển khai trên VPS/Server chuyên dụng
chạy TCP MQTT Broker liên tục (không phù hợp với Serverless Vercel).
"""

import json
import threading
from database import insert_data
from twin_model import battery_twin_instance

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None

BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC_DATA = "datn/battery/data"
TOPIC_CMD = "datn/battery/cmd"

mqtt_client = None

def on_connect(client, userdata, flags, rc):
    print("MQTT Connected successfully to", BROKER)
    client.subscribe(TOPIC_DATA)

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        v = float(data.get('V', 0.0))
        i = float(data.get('I', 0.0))
        t = float(data.get('T', 0.0))
        soc = float(data.get('SOC', 0.0))
        soh = float(data.get('SOH', 100.0))
        
        # Calculate Power and simple Energy
        p = float(data.get('P', round(v * i, 3)))
        energy = float(data.get('E', round(p * (1/3600), 4)))
        
        # Chuẩn hóa quy ước chiều dòng điện: I > 0 là xả, I < 0 là sạc
        status = data.get('Status') or ("Discharging" if i > 0.01 else ("Charging" if i < -0.01 else "Idle"))
        
        # Đồng bộ Thực thể số (Digital Twin)
        battery_twin_instance.sync(data)
        twin_state = battery_twin_instance.twin_state
        
        # Insert into DB
        insert_data(
            v, i, p, energy, t, soc, soh, status,
            twin_ocv=twin_state.get('ocv', 0.0),
            twin_r0=twin_state.get('internal_resistance', 0.0),
            twin_vp=twin_state.get('polarization_voltage', 0.0),
            ai_class=data.get('AI_Class', 0),
            ai_score=data.get('AI_Score', 0.0),
            ai_time=data.get('AI_Time', 0),
            cv=data.get('CV', [0.0, 0.0, 0.0, 0.0]),
            csoc=data.get('CSOC', [0, 0, 0, 0]),
            csoh=data.get('CSOH', [100, 100, 100, 100]),
            cai_class=data.get('CAI_Class', [0, 0, 0, 0]),
            fault_flags=data.get('Fault_Flags', 'NORMAL')
        )
        
    except Exception as e:
        print("MQTT Error:", e)

def start_mqtt():
    global mqtt_client
    if mqtt is None:
        print("Cảnh báo: Thư viện paho-mqtt chưa được cài đặt. MQTT Worker không khởi động.")
        return

    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    
    def run_loop():
        try:
            mqtt_client.connect(BROKER, PORT, 60)
            mqtt_client.loop_forever()
        except Exception as e:
            print("MQTT Connection Error:", e)
        
    thread = threading.Thread(target=run_loop, daemon=True)
    thread.start()

def send_command(command):
    if mqtt_client:
        mqtt_client.publish(TOPIC_CMD, command)
