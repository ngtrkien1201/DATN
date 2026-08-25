import os
from datetime import datetime
from pymongo import MongoClient

# Dùng biến môi trường MONGO_URI trên Vercel, nếu không có thì chạy local (dành cho test)
MONGO_URI = os.environ.get('MONGO_URI', 'mongodb://localhost:27017/')
client = MongoClient(MONGO_URI)
db = client['battery_digital_twin']
collection = db['battery_data']
twin_state_collection = db['twin_state'] # Dùng để lưu trạng thái Digital Twin giữa các lần request

def init_db():
    # MongoDB tự động tạo db và collection khi có dữ liệu nên không cần CREATE TABLE
    pass

def insert_data(voltage, current, power, energy, temperature, soc, soh, status, twin_ocv=0.0, twin_r0=0.0, twin_vp=0.0, ai_class=0, ai_score=0.0, ai_time=0):
    data = {
        "timestamp": datetime.utcnow().isoformat() + "Z",
        "voltage": voltage,
        "current": current,
        "power": power,
        "energy": energy,
        "temperature": temperature,
        "soc": soc,
        "soh": soh,
        "status": status,
        "twin_ocv": twin_ocv,
        "twin_r0": twin_r0,
        "twin_vp": twin_vp,
        "ai_class": ai_class,
        "ai_score": ai_score,
        "ai_time": ai_time
    }
    collection.insert_one(data)

def get_latest_data():
    # Lấy document mới nhất dựa trên object ID (được tạo tăng dần theo thời gian)
    doc = collection.find_one(sort=[('_id', -1)])
    if doc:
        doc['_id'] = str(doc['_id'])
    return doc

def get_history(limit=50):
    # Lấy history cũ nhất để vẽ biểu đồ (sort tăng dần)
    # Tuy nhiên, để lấy N phần tử gần nhất thì phải sort giảm dần rồi reverse, hoặc sort giảm dần lấy limit rồi reverse
    docs = list(collection.find().sort('_id', -1).limit(limit))
    docs.reverse()
    for doc in docs:
         doc['_id'] = str(doc['_id'])
    return docs

def get_all_history():
    # Lấy toàn bộ dữ liệu lịch sử tăng dần theo thời gian
    docs = list(collection.find().sort('_id', 1))
    for doc in docs:
         doc['_id'] = str(doc['_id'])
    return docs

def clear_all_history():
    # Xoá toàn bộ dữ liệu lịch sử
    collection.delete_many({})
    return True

def save_twin_state(state_dict):
    """ Lưu trạng thái Digital Twin vào DB (Vercel cần cái này vì RAM bị xóa sau mỗi request) """
    twin_state_collection.replace_one({"_id": "current_state"}, {"_id": "current_state", "state": state_dict}, upsert=True)

def load_twin_state():
    """ Tải lại trạng thái """
    doc = twin_state_collection.find_one({"_id": "current_state"})
    if doc and "state" in doc:
        return doc["state"]
    return None

def save_pending_command(command_str):
    """ Lưu một lệnh chờ xuống DB để ESP32 lấy """
    db['pending_commands'].insert_one({
        "command": command_str,
        "timestamp": datetime.utcnow()
    })

def pop_pending_command():
    """ Lấy ra lệnh chờ cũ nhất và xóa nó (FIFO) """
    # Tìm lệnh cũ nhất
    doc = db['pending_commands'].find_one_and_delete(
        {}, 
        sort=[("timestamp", 1)]
    )
    if doc:
        return doc.get("command")
    return None
