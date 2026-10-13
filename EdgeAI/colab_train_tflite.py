import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Dropout
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.preprocessing import LabelEncoder
import os

# ==============================================================================
# HƯỚNG DẪN: SỬ DỤNG TRÊN GOOGLE COLAB
# 1. Export CSV từ: https://datn-mauve.vercel.app/api/export-csv
# 2. Thêm cột "Label" vào file CSV với nhãn:
#    Normal | Over-voltage | Under-voltage | Over-current | Over-heat | Cell-Imbalance
# 3. Đổi tên file thành: battery_dataset_4s.csv
# 4. Upload lên Google Colab và chạy script này
#
# FEATURES (10): Cell1_V, Cell2_V, Cell3_V, Cell4_V,
#                Cell1_SOC, Cell2_SOC, Cell3_SOC, Cell4_SOC,
#                Pack_I, Pack_T
# LABELS  (6) : Normal, Over-voltage, Under-voltage, Over-current, Over-heat, Cell-Imbalance
# ==============================================================================

csv_file = 'battery_dataset_4s.csv'
if not os.path.exists(csv_file):
    print(f"Vui lòng tải file '{csv_file}' lên Google Colab!")
else:
    df = pd.read_csv(csv_file, sep=None, engine='python')
    df.columns = df.columns.str.strip()
    print("Các cột trong file:", df.columns.tolist())

    # 1. FEATURES & LABELS
    feature_cols = ['Cell1_V', 'Cell2_V', 'Cell3_V', 'Cell4_V',
                    'Cell1_SOC', 'Cell2_SOC', 'Cell3_SOC', 'Cell4_SOC',
                    'Pack_I', 'Pack_T']
    try:
        X = df[feature_cols].values
    except KeyError as e:
        print(f"\n[LỖI] Thiếu cột: {e}")
        print("Các cột hiện có:", df.columns.tolist())
        raise e

    label_encoder = LabelEncoder()
    y = label_encoder.fit_transform(df['Label'].values)

    print("\nClass Mapping:")
    for i, name in enumerate(label_encoder.classes_):
        print(f"  Class {i}: {name}")

    num_classes = len(np.unique(y))

    # 2. TRAIN / TEST SPLIT
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y)

    # 3. CHUẨN HÓA (StandardScaler)
    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled  = scaler.transform(X_test)

    print("\n==== THÔNG SỐ SCALER (Copy vào Gateway.ino) ====")
    feature_names = ['V_c1','V_c2','V_c3','V_c4',
                     'SOC_c1','SOC_c2','SOC_c3','SOC_c4',
                     'I_pack','T_pack']
    for i, name in enumerate(feature_names):
        print(f"  {name}: mean={scaler.mean_[i]:.6f}, scale={scaler.scale_[i]:.6f}")
    print("=================================================\n")

    # 4. XÂY DỰNG NEURAL NETWORK PACK-LEVEL (10 → 32 → 16 → 6)
    model = Sequential([
        Dense(32, activation='relu', input_shape=(10,)),
        Dropout(0.2),
        Dense(16, activation='relu'),
        Dense(num_classes, activation='softmax')
    ])

    model.compile(optimizer='adam',
                  loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])

    model.summary()

    # 5. HUẤN LUYỆN
    print("\nBắt đầu huấn luyện model pack-level 4S...")
    model.fit(X_train_scaled, y_train,
              epochs=80, batch_size=32,
              validation_data=(X_test_scaled, y_test))

    loss, acc = model.evaluate(X_test_scaled, y_test)
    print(f"\nĐộ chính xác trên tập Test: {acc*100:.2f}%")

    # 6. CHUYỂN ĐỔI SANG TFLITE + INT8 QUANTIZATION
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    def representative_dataset():
        for i in range(len(X_train_scaled)):
            yield [X_train_scaled[i:i+1].astype(np.float32)]

    converter.representative_dataset = representative_dataset
    tflite_model = converter.convert()

    with open("anomaly_model_4s.tflite", "wb") as f:
        f.write(tflite_model)
    print(f"Kích thước model TFLite: {len(tflite_model)} bytes")

    # 7. XUẤT THÀNH C ARRAY (model_data.h) CHO ESP32
    def to_c_array(tflite_path, out_path):
        with open(tflite_path, 'rb') as f:
            data = f.read()
        c = "#ifndef MODEL_DATA_H\n#define MODEL_DATA_H\n\n"
        c += "// Pack-level 4S Battery Anomaly Detection Model\n"
        c += f"// Input:  10 features [V_c1..V_c4, SOC_c1..SOC_c4, I, T]\n"
        c += f"// Output: {num_classes} classes\n"
        c += f"// Size:   {len(data)} bytes\n\n"
        c += "const unsigned char anomaly_model_tflite[] = {\n    "
        for i, byte in enumerate(data):
            c += f"0x{byte:02x}, "
            if (i + 1) % 12 == 0:
                c += "\n    "
        c += "\n};\n\n"
        c += f"const int anomaly_model_tflite_len = {len(data)};\n\n"
        c += "#endif // MODEL_DATA_H\n"
        with open(out_path, "w") as f:
            f.write(c)

    to_c_array("anomaly_model_4s.tflite", "model_data.h")
    print("\n[THÀNH CÔNG] Đã tạo file model_data.h")
    print("Tải về và copy vào: ESP32_Gateway/Gateway/model_data.h")
    print("\n⚠️  Nhớ cập nhật thông số Scaler ở trên vào Gateway.ino!")



