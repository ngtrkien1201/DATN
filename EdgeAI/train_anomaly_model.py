import os
import random
import numpy as np
from sklearn.tree import DecisionTreeClassifier
from micromlgen import port

# ==========================================
# BƯỚC 1: TẠO DỮ LIỆU MÔ PHỎNG (SYNTHETIC DATASET - CHUẨN HÓA MỚI)
# Các đặc trưng (Features): [Voltage(V), Current(A), Temperature(C), SOC(%)]
# ==========================================
print("1. Generating synthetic dataset with optimized thresholds...")
X = []
y = []

# Class 0: Normal Operation (Hoạt động bình thường)
# V: 3.30 -> 4.20V | I: -1.0 -> 1.0A | T: 20 -> 40°C | SOC: 20 -> 100%
for _ in range(600):
    X.append([random.uniform(3.30, 4.20), random.uniform(-1.0, 1.0), random.uniform(20.0, 40.0), random.uniform(20, 100)])
    y.append(0)

# Class 1: Over-voltage (Quá áp)
# V: 4.20 -> 4.35V | SOC: 95 -> 100%
for _ in range(150):
    X.append([random.uniform(4.20, 4.35), random.uniform(-1.0, 1.0), random.uniform(25.0, 40.0), random.uniform(95, 100)])
    y.append(1)

# Class 2: Under-voltage / Deep Discharge (Sụt áp / Xả sâu)
# V: 2.50 -> 3.00V | SOC: 0 -> 15%
for _ in range(150):
    X.append([random.uniform(2.50, 3.00), random.uniform(-1.0, 1.0), random.uniform(25.0, 40.0), random.uniform(0, 15)])
    y.append(2)

# Class 3: Over-current (Quá dòng)
# |I| > 2.0A (dựa theo C-rate 1C của pack 2200mAh)
for _ in range(150):
    current = random.uniform(2.0, 3.5) if random.random() > 0.5 else random.uniform(-3.5, -2.0)
    X.append([random.uniform(3.30, 4.20), current, random.uniform(30.0, 45.0), random.uniform(20, 90)])
    y.append(3)

# Class 4: Over-temperature (Quá nhiệt)
# T: 50 -> 70°C (Fault threshold)
for _ in range(150):
    X.append([random.uniform(3.30, 4.20), random.uniform(-1.0, 1.0), random.uniform(50.0, 70.0), random.uniform(20, 90)])
    y.append(4)

# Class 5: Cell Imbalance (Lệch cell > 200mV khi SOC > 30%)
# Mô phỏng cell yếu có V sụt sâu (> 200mV so với mức bình thường) nhưng vẫn > 3.00V để không vi phạm Under-voltage (Ưu tiên 2)
for _ in range(150):
    weak_v   = random.uniform(3.05, 3.40)
    weak_soc = random.uniform(35, 80)
    X.append([weak_v, random.uniform(-0.5, 0.5), random.uniform(20.0, 40.0), weak_soc])
    y.append(5)

X = np.array(X)
y = np.array(y)

# ==========================================
# BƯỚC 2: HUẤN LUYỆN MÔ HÌNH (TRAIN MODEL)
# ==========================================
print("2. Training Decision Tree model...")
# Dùng Decision Tree vì nó cực kỳ nhẹ, suy luận rất nhanh bằng các lệnh if-else trên vi điều khiển
clf = DecisionTreeClassifier(max_depth=5, random_state=42)
clf.fit(X, y)

accuracy = clf.score(X, y)
print(f"   -> Training Accuracy: {accuracy * 100:.2f}%")

# ==========================================
# BƯỚC 3: XUẤT CODE C/C++ CHO ESP32 (PORTING)
# ==========================================
print("3. Exporting to C++ header file...")
c_code = port(clf)

# Lưu thành file AnomalyModel.h
output_path = os.path.join(os.path.dirname(__file__), 'AnomalyModel.h')
with open(output_path, "w") as f:
    f.write(c_code)

print(f"\n[THÀNH CÔNG] Đã lưu mô hình nhúng tại: {output_path}")
print("Hãy copy file AnomalyModel.h này vào chung thư mục chứa file Gateway.ino của ESP32!")
