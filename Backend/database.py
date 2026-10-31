import os
import logging
from datetime import datetime
from pymongo import MongoClient
from pymongo.errors import PyMongoError, ServerSelectionTimeoutError

# Dùng biến môi trường MONGO_URI trên Vercel, nếu không có thì chạy local (in-memory siêu tốc)
MONGO_URI = os.environ.get('MONGO_URI')

_use_mongo = False
client = None
db = None
collection = None
twin_state_collection = None

if MONGO_URI:
    try:
        # Timeout ngắn (1500ms) để nếu MongoDB Atlas chập chờn thì không block quá lâu
        client = MongoClient(MONGO_URI, serverSelectionTimeoutMS=1500)
        client.admin.command('ping')
        db = client['battery_digital_twin']
        collection = db['battery_data']
        twin_state_collection = db['twin_state']
        _use_mongo = True
    except Exception as e:
        logging.warning("MongoDB ping failed: %s -> Chuyển sang In-Memory fallback.", e)
        _use_mongo = False

# Bộ nhớ tạm in-memory dự phòng (Fallback nếu MongoDB mất kết nối hoặc chạy local)
_in_memory_history = []
_in_memory_twin_state = {}
_in_memory_pending_commands = []

def init_db():
    pass

def insert_data(voltage, current, power, energy, temperature, soc, soh, status, twin_ocv=0.0, twin_r0=0.0, twin_vp=0.0, ai_class=0, ai_score=0.0, ai_time=0, cv=None, csoc=None, csoh=None, cai_class=None, fault_flags="NORMAL"):
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
        "ai_time": ai_time,
        "fault_flags": fault_flags,
        "cv": cv if cv is not None else [0.0, 0.0, 0.0, 0.0],
        "csoc": csoc if csoc is not None else [0, 0, 0, 0],
        "csoh": csoh if csoh is not None else [100, 100, 100, 100],
        "cai_class": cai_class if cai_class is not None else [0, 0, 0, 0]
    }
    
    # Lưu vào in-memory cache dự phòng trước
    _in_memory_history.append(data.copy())
    if len(_in_memory_history) > 100:
        _in_memory_history.pop(0)

    if _use_mongo and collection is not None:
        try:
            collection.insert_one(data)
        except Exception as e:
            logging.warning("MongoDB insert error: %s", e)

def get_latest_data():
    if _use_mongo and collection is not None:
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
    if _use_mongo and collection is not None:
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

def get_all_history():
    if _use_mongo and collection is not None:
        try:
            docs = list(collection.find().sort('_id', 1))
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
    if _use_mongo and collection is not None:
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
    if _use_mongo and twin_state_collection is not None:
        try:
            twin_state_collection.replace_one({"_id": "current_state"}, {"_id": "current_state", "state": state_dict}, upsert=True)
        except Exception as e:
            logging.warning("MongoDB save_twin_state error: %s", e)

def load_twin_state():
    if _use_mongo and twin_state_collection is not None:
        try:
            doc = twin_state_collection.find_one({"_id": "current_state"})
            if doc and "state" in doc:
                return doc["state"]
        except Exception as e:
            logging.warning("MongoDB load_twin_state error: %s", e)

    return _in_memory_twin_state if _in_memory_twin_state else None

def save_pending_command(command_str):
    _in_memory_pending_commands.append(command_str)
    if _use_mongo and db is not None:
        try:
            db['pending_commands'].insert_one({
                "command": command_str,
                "timestamp": datetime.utcnow()
            })
        except Exception as e:
            logging.warning("MongoDB save_pending_command error: %s", e)

def pop_pending_command():
    if _use_mongo and db is not None:
        try:
            doc = db['pending_commands'].find_one_and_delete(
                {}, 
                sort=[("timestamp", 1)]
            )
            if doc:
                return doc.get("command")
        except Exception as e:
            logging.warning("MongoDB pop_pending_command error: %s", e)

    if _in_memory_pending_commands:
        return _in_memory_pending_commands.pop(0)
    return None
