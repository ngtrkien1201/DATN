import os
import random
import numpy as np
from sklearn.tree import DecisionTreeClassifier
from micromlgen import port

# ==========================================
# BƯỚC 1: TẠO DỮ LIỆU MÔ PHỎNG (SYNTHETIC DATASET)
# Các đặc trưng (Features): [Voltage(V), Current(A), Temperature(C), SOC(%)]
# ==========================================
print("1. Generating synthetic dataset...")
X = []
y = []

# Class 0: Normal Operation (Hoạt động bình thường)
# V: 3.0 -> 4.2 | I: -1.0 -> 1.0 | T: 20 -> 40 | SOC: 10 -> 100
for _ in range(500):
    X.append([random.uniform(3.0, 4.2), random.uniform(-1.0, 1.0), random.uniform(20.0, 40.0), random.uniform(10, 100)])
    y.append(0)

# Class 1: Over-voltage (Quá áp)
# V: 4.25 -> 4.5 | I: ... | T: ... | SOC: ...
for _ in range(150):
    X.append([random.uniform(4.25, 4.5), random.uniform(-1.0, 1.0), random.uniform(25.0, 40.0), random.uniform(90, 100)])
    y.append(1)

# Class 2: Under-voltage / Deep Discharge (Sụt áp)
# V: 2.0 -> 2.7 | I: ... | T: ... | SOC: ...
for _ in range(150):
    X.append([random.uniform(2.0, 2.7), random.uniform(-1.0, 1.0), random.uniform(25.0, 40.0), random.uniform(0, 10)])
    y.append(2)

# Class 3: Over-current (Quá dòng)
# I > 1.5A hoặc I < -1.5A
for _ in range(150):
    current = random.uniform(1.5, 3.0) if random.random() > 0.5 else random.uniform(-3.0, -1.5)
    X.append([random.uniform(3.0, 4.2), current, random.uniform(35.0, 50.0), random.uniform(10, 90)])
    y.append(3)

# Class 4: Over-temperature (Quá nhiệt)
# T > 45 C
for _ in range(150):
    X.append([random.uniform(3.0, 4.2), random.uniform(1.0, 2.0), random.uniform(46.0, 65.0), random.uniform(10, 90)])
    y.append(4)

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
