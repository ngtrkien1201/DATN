import paho.mqtt.client as mqtt
import json
import threading
from database import insert_data
from twin_model import battery_twin_instance

BROKER = "broker.hivemq.com"
PORT = 8000  # Default WebSocket port or normal TCP port depending on paho transport
TOPIC_DATA = "datn/battery/data"
TOPIC_CMD = "datn/battery/cmd"

mqtt_client = None

def on_connect(client, userdata, flags, rc):
    print("MQTT Connected!")
    client.subscribe(TOPIC_DATA)

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
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
        
        # Đồng bộ Thực thể số (Digital Twin)
        battery_twin_instance.sync(data)
        
    except Exception as e:
        print("MQTT Error:", e)

def start_mqtt():
    global mqtt_client
    # HiveMQ over TCP
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    
    # Run in background thread
    def run_loop():
        mqtt_client.connect(BROKER, 1883, 60)
        mqtt_client.loop_forever()
        
    thread = threading.Thread(target=run_loop, daemon=True)
    thread.start()

def send_command(command):
    if mqtt_client:
        mqtt_client.publish(TOPIC_CMD, command)
