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
# 1. Tải file CSV của bạn lên Google Colab
# 2. File CSV cần có các cột: V real, I real, T real, SOC real, Label
# 3. Chạy đoạn code này để Train và xuất ra mảng byte C++ cho ESP32.
# ==============================================================================

# 1. TẢI VÀ CHUẨN BỊ DỮ LIỆU
csv_file = 'battery_dataset_augmented.csv'
if not os.path.exists(csv_file):
    print(f"Vui lòng tải file {csv_file} lên Google Colab!")
else:
    # Tự động nhận diện dấu phẩy hoặc chấm phẩy (rất hay gặp khi xuất từ Excel VN)
    df = pd.read_csv(csv_file, sep=None, engine='python')
    
    # Xóa khoảng trắng thừa ở đầu/cuối tên cột để tránh lỗi KeyError
    df.columns = df.columns.str.strip()
    
    try:
        # Chia tính năng (Features) và nhãn (Labels)
        X = df[['V_real', 'I_real', 'T_real', 'SOC_real']].values
    except KeyError as e:
        print("\n[LỖI NGHIÊM TRỌNG] Tên cột trong file của bạn không khớp!")
        print("Danh sách các cột đang có thực sự trong file là:", df.columns.tolist())
        raise e
    
    # Mã hóa nhãn dạng chữ (ví dụ: 'Normal', 'Over-heat') sang số nguyên (0, 1, 2...)
    label_encoder = LabelEncoder()
    y = label_encoder.fit_transform(df['Label'].values)
    
    print("Danh sách các nhãn (Class Mapping):")
    for i, class_name in enumerate(label_encoder.classes_):
        print(f"  Class {i}: {class_name}")
    
    # Số lượng phân lớp (Classes)
    num_classes = len(np.unique(y))
    
    # Chia tập Train / Test
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
    
    # Chuẩn hóa dữ liệu (QUAN TRỌNG: Phải lưu lại thông số scaler để code C++ scale y hệt)
    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled = scaler.transform(X_test)
    
    print("\nThông số Scaler (Mean):", scaler.mean_)
    print("Thông số Scaler (Scale):", scaler.scale_)
    
    # 2. XÂY DỰNG MÔ HÌNH NEURAL NETWORK (MLP)
    # Mô hình nhỏ gọn để vừa với RAM của ESP32 (Tensor Arena)
    model = Sequential([
        Dense(16, activation='relu', input_shape=(4,)),
        Dense(8, activation='relu'),
        Dense(num_classes, activation='softmax')
    ])
    
    model.compile(optimizer='adam', 
                  loss='sparse_categorical_crossentropy', 
                  metrics=['accuracy'])
    
    # 3. HUẤN LUYỆN
    print("\nBắt đầu huấn luyện mô hình TensorFlow...")
    model.fit(X_train_scaled, y_train, epochs=50, batch_size=32, validation_data=(X_test_scaled, y_test))
    
    # Đánh giá
    loss, acc = model.evaluate(X_test_scaled, y_test)
    print(f"\nĐộ chính xác trên tập Test: {acc*100:.2f}%")
    
    # 4. CHUYỂN ĐỔI SANG TENSORFLOW LITE (TFLITE) VÀ QUANTIZATION
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    
    def representative_dataset():
        for i in range(len(X_train_scaled)):
            yield [X_train_scaled[i:i+1].astype(np.float32)]
            
    converter.representative_dataset = representative_dataset
    tflite_model = converter.convert()
    
    # Lưu file .tflite
    tflite_filename = "anomaly_model.tflite"
    with open(tflite_filename, "wb") as f:
        f.write(tflite_model)
    
    # 5. CHUYỂN ĐỔI TFLITE THÀNH MẢNG C-BYTE (C ARRAY) CHO ESP32
    # Hàm này mô phỏng công cụ 'xxd -i' của Linux
    def convert_tflite_to_c_array(tflite_path, c_file_path):
        with open(tflite_path, 'rb') as f:
            model_bytes = f.read()
        
        c_code = "#ifndef MODEL_DATA_H\n#define MODEL_DATA_H\n\n"
        c_code += "const unsigned char anomaly_model_tflite[] = {\n    "
        
        for i, byte in enumerate(model_bytes):
            c_code += f"0x{byte:02x}, "
            if (i + 1) % 12 == 0:
                c_code += "\n    "
                
        c_code += "\n};\n\n"
        c_code += f"const int anomaly_model_tflite_len = {len(model_bytes)};\n\n"
        c_code += "#endif // MODEL_DATA_H\n"
        
        with open(c_file_path, "w") as f:
            f.write(c_code)
            
    convert_tflite_to_c_array(tflite_filename, "model_data.h")
    print("\n[THÀNH CÔNG] Đã tạo file model_data.h")
    print("Vui lòng tải file 'model_data.h' về và đưa vào thư mục ESP32_Gateway/Gateway/")
