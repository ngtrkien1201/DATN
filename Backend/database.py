import os
import logging
from datetime import datetime, timezone
from pymongo import MongoClient
from pymongo.errors import PyMongoError, ServerSelectionTimeoutError

# Dùng biến môi trường MONGO_URI trên Vercel, nếu không có thì chạy local (in-memory siêu tốc)
MONGO_URI = os.environ.get('MONGO_URI')

_use_mongo = False
client = None
db = None
collection = None
twin_state_collection = None

# Bộ nhớ tạm in-memory dự phòng (Fallback nếu MongoDB mất kết nối hoặc chạy local)
_in_memory_history = []
_in_memory_twin_state = {}
_in_memory_pending_commands = []

def ensure_mongo_connection():
    """Tự động kết nối hoặc phục hồi kết nối tới MongoDB Atlas"""
    global _use_mongo, client, db, collection, twin_state_collection
    if not MONGO_URI:
        _use_mongo = False
        return False
        
    if _use_mongo and collection is not None:
        return True

    try:
        # Timeout 3000ms hợp lý cho DNS SRV và TLS handshake trên Serverless
        client = MongoClient(MONGO_URI, serverSelectionTimeoutMS=3000, connectTimeoutMS=3000)
        client.admin.command('ping')
        db = client['battery_digital_twin']
        collection = db['battery_data']
        twin_state_collection = db['twin_state']
        _use_mongo = True
        return True
    except Exception as e:
        logging.warning("MongoDB connect/ping failed: %s -> Chuyển sang In-Memory fallback.", e)
        _use_mongo = False
        return False

# Thử kết nối lần đầu khi nạp module
ensure_mongo_connection()

def init_db():
    ensure_mongo_connection()

def insert_data(voltage, current, power, energy, temperature, soc, soh, status, twin_ocv=0.0, twin_r0=0.0, twin_vp=0.0, ai_class=0, ai_score=0.0, ai_time=0, cv=None, csoc=None, csoh=None, cai_class=None, fault_flags="NORMAL"):
    now_iso = datetime.now(timezone.utc).isoformat()
    data = {
        "timestamp": now_iso,
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
        "ai_time": ai_time,
        "fault_flags": fault_flags,
        "cv": cv if cv is not None else [0.0, 0.0, 0.0, 0.0],
        "csoc": csoc if csoc is not None else [0, 0, 0, 0],
        "csoh": csoh if csoh is not None else [100, 100, 100, 100],
        "cai_class": cai_class if cai_class is not None else [0, 0, 0, 0]
    }
    
    # Lưu vào in-memory cache dự phòng trước
    _in_memory_history.append(data.copy())
    if len(_in_memory_history) > 200:
        _in_memory_history.pop(0)

    if ensure_mongo_connection() and collection is not None:
        try:
            mongo_doc = data.copy()
            collection.insert_one(mongo_doc)
        except Exception as e:
            logging.warning("MongoDB insert error: %s", e)

def get_latest_data():
    if ensure_mongo_connection() and collection is not None:
        try:
            doc = collection.find_one(sort=[('_id', -1)])
            if doc:
                doc['_id'] = str(doc['_id'])
                return doc
        except Exception as e:
            logging.warning("MongoDB get_latest_data error: %s", e)

    # Fallback in-memory
    if _in_memory_history:
        return _in_memory_history[-1]
    return None

def get_history(limit=50):
    if ensure_mongo_connection() and collection is not None:
        try:
            docs = list(collection.find().sort('_id', -1).limit(limit))
            docs.reverse()
            for doc in docs:
                doc['_id'] = str(doc['_id'])
            if docs:
                return docs
        except Exception as e:
            logging.warning("MongoDB get_history error: %s", e)

    # Fallback in-memory
    return _in_memory_history[-limit:]

def get_all_history(limit=5000):
    """Lấy toàn bộ lịch sử (có giới hạn an toàn để chống tràn RAM trên Serverless)"""
    if ensure_mongo_connection() and collection is not None:
        try:
            docs = list(collection.find().sort('_id', 1).limit(limit))
            for doc in docs:
                doc['_id'] = str(doc['_id'])
            if docs:
                return docs
        except Exception as e:
            logging.warning("MongoDB get_all_history error: %s", e)

    # Fallback in-memory
    return _in_memory_history

def clear_all_history():
    global _in_memory_history
    _in_memory_history = []
    if ensure_mongo_connection() and collection is not None:
        try:
            collection.delete_many({})
            return True
        except Exception as e:
            logging.warning("MongoDB clear_all_history error: %s", e)
            return False
    return True

def save_twin_state(state_dict):
    global _in_memory_twin_state
    _in_memory_twin_state = state_dict.copy()
    if ensure_mongo_connection() and twin_state_collection is not None:
        try:
            twin_state_collection.replace_one(
                {"_id": "current_state"}, 
                {"_id": "current_state", "state": state_dict}, 
                upsert=True
            )
        except Exception as e:
            logging.warning("MongoDB save_twin_state error: %s", e)

def load_twin_state():
    if ensure_mongo_connection() and twin_state_collection is not None:
        try:
            doc = twin_state_collection.find_one({"_id": "current_state"})
            if doc and "state" in doc:
                return doc["state"]
        except Exception as e:
            logging.warning("MongoDB load_twin_state error: %s", e)

    return _in_memory_twin_state if _in_memory_twin_state else None

def save_pending_command(command_str):
    global _in_memory_pending_commands
    _in_memory_pending_commands.clear()
    _in_memory_pending_commands.append(command_str)
    if ensure_mongo_connection() and db is not None:
        try:
            # Xóa các lệnh cũ để lệnh mới nhất được thực thi ngay lập tức, không bị trễ hàng đợi
            db['pending_commands'].delete_many({})
            db['pending_commands'].insert_one({
                "command": command_str,
                "timestamp": datetime.now(timezone.utc)
            })
        except Exception as e:
            logging.warning("MongoDB save_pending_command error: %s", e)

def pop_pending_command():
    global _in_memory_pending_commands
    if ensure_mongo_connection() and db is not None:
        try:
            doc = db['pending_commands'].find_one(sort=[("timestamp", -1)])
            if doc:
                db['pending_commands'].delete_many({})
                return doc.get("command")
        except Exception as e:
            logging.warning("MongoDB pop_pending_command error: %s", e)

    if _in_memory_pending_commands:
        cmd = _in_memory_pending_commands[-1]
        _in_memory_pending_commands.clear()
        return cmd
    return None
