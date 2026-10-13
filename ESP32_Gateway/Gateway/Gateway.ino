#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ================= CẤU HÌNH TENSORFLOW LITE =================
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model_data.h"

// Khởi tạo các đối tượng TFLite
tflite::MicroErrorReporter micro_error_reporter;
tflite::ErrorReporter* error_reporter = &micro_error_reporter;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

constexpr int kTensorArenaSize = 16384; // Tăng lên 16KB cho model pack-level 10 features
uint8_t tensor_arena[kTensorArenaSize];
// ============================================================

// ================= CẤU HÌNH WIFI =================
const char* ssid     = "LinhTrung_3";   // Thay tên WiFi của bạn vào đây
const char* password = "23012024";      // Thay mật khẩu WiFi vào đây

// ================= CẤU HÌNH SERVER VERCEL =================
const char* serverUrl = "https://datn-mauve.vercel.app/api/telemetry";

// ================= CẤU HÌNH UART (KẾT NỐI STM32) =================
#define RX_PIN 16
#define TX_PIN 17
#define UART_BAUD 115200

// Danh mục phân lớp Anomaly của mô hình AI
const char* AI_CLASS_NAMES[] = {
    "Normal Operation",
    "Over-voltage",
    "Under-voltage",
    "Over-current",
    "Over-heat",
    "Cell Imbalance"   // Class 5: Mất cân bằng áp giữa các cell
};

// Cấu hình ngưỡng bảo vệ vật lý an toàn (Safety Guardrails) cho Pack 4S
#define CELL_OV_THRESHOLD   4.25f   // Quá áp Cell (> 4.25V)
#define CELL_UV_THRESHOLD   2.80f   // Sụt áp Cell (< 2.80V)
#define PACK_OC_THRESHOLD   3.00f   // Quá dòng Pack (|I| > 3.0A)
#define PACK_OT_THRESHOLD   55.0f   // Quá nhiệt Pack (T > 55.0°C)
#define CELL_IMBALANCE_MAX  0.35f   // Độ lệch áp tối đa giữa các cell (350mV)

WiFiClientSecure secureClient;
unsigned long lastWifiCheck = 0;
unsigned long lastWaitingMsg = 0;

void setup_wifi() 
{
    Serial.println();
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    // Chờ tối đa 10 giây để không gây block toàn hệ thống nếu mất mạng
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) 
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected successfully!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed! Will retry in background...");
    }
}

void setup() 
{
    Serial.begin(115200);
    delay(2000); // Chờ cổng USB Serial kết nối với máy tính
    
    // Mở rộng RX Buffer của Serial1 lên 2048 bytes để không bị tràn khi thực hiện HTTP POST
    Serial1.setRxBufferSize(2048);
    Serial1.begin(UART_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial1.setTimeout(50); // Timeout ngắn để tránh block
    
    Serial.println("\n\n==================================================");
    Serial.println("   ESP32 GATEWAY - 4S BATTERY MANAGEMENT SYSTEM   ");
    Serial.println("==================================================");
    
    Serial.println("Khởi động Edge AI (TensorFlow Lite Micro)...");
    model = tflite::GetModel(anomaly_model_tflite);
    
    static tflite::AllOpsResolver micro_op_resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;
    
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.println("LỖI: Không đủ bộ nhớ Tensor Arena! Hãy tăng kTensorArenaSize.");
        return;
    }
    
    input = interpreter->input(0);
    output = interpreter->output(0);
    
    if (input == nullptr || output == nullptr) {
        Serial.println("LỖI: Không lấy được con trỏ Input/Output của Model!");
        return;
    }
    
    Serial.println("-> Edge AI Model sẵn sàng!");
    secureClient.setInsecure(); // Tắt kiểm tra SSL cert để tăng tốc và ổn định
    setup_wifi();
}

void processBatteryTelemetry(String jsonString)
{
    // Cấp phát bộ nhớ đủ lớn cho cấu trúc JSON 4S (1024 bytes)
    #if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc;
    #else
        DynamicJsonDocument doc(1024);
    #endif
    
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.print("-> [ERROR] Lỗi Parse JSON từ STM32: ");
        Serial.println(error.c_str());
        return;
    }

    // Đọc các giá trị tổng thể Pack
    float pack_v = doc["V"] | 0.0f;
    float I      = doc["I"] | 0.0f;
    float T      = doc["T"] | 0.0f;
    int pack_soc = doc["SOC"] | 0;
    int pack_soh = doc["SOH"] | 100;

    // === KIỂM TRA PIN ĐÃ LẮP CHƯA ===
    // Nếu Pack Voltage < 1.0V → STM32 đang chạy bằng USB nhưng chưa lắp pin
    // Bỏ qua AI inference để tránh báo Under-voltage giả
    if (pack_v < 1.0f) {
        Serial.println("-> [GATEWAY] Chưa lắp pin (Pack V < 1.0V). Bỏ qua AI & HTTP POST.");
        return;
    }
    // ===================================

    // Trích xuất mảng điện áp (CV) và SOC (CSOC) của 4 Cell
    JsonArray cv = doc["CV"];
    JsonArray csoc = doc["CSOC"];

    if (!cv || !csoc || cv.size() < 4) {
        Serial.println("-> [WARNING] Gói tin không chứa đầy đủ dữ liệu 4 Cell (CV/CSOC)!");
        return;
    }

    // ============================================================
    // BƯỚC 1: EDGE AI INFERENCE CẤP PACK 4S (1 LẦN DUY NHẤT)
    // Input: [V_c1, V_c2, V_c3, V_c4, SOC_c1, SOC_c2, SOC_c3, SOC_c4, I, T] = 10 features
    // Output: 6 classes [Normal, OV, UV, OC, OT, Imbalance]
    // ============================================================
    int pack_ai_class = 0;
    float pack_ai_score = 100.0f;
    unsigned long total_inference_time_us = 0;

    float min_cell_v = cv[0].as<float>();
    float max_cell_v = cv[0].as<float>();
    for (int i = 1; i < 4; i++) {
        float v = cv[i].as<float>();
        if (v < min_cell_v) min_cell_v = v;
        if (v > max_cell_v) max_cell_v = v;
    }
    float cell_imbalance = max_cell_v - min_cell_v;

    if (interpreter != nullptr && input != nullptr && output != nullptr) {
        // ==============================================================
        // CHUẨN HÓA DỮ LIỆU 4S (StandardScaler)
        // ⚠️ CẬP NHẬT các giá trị mean/scale sau khi train lại trên Colab!
        // Lấy từ output: print("Mean:", scaler.mean_) | print("Scale:", scaler.scale_)
        // ==============================================================
        const float V_MEAN   = 3.50f,  V_SCALE   = 0.38f;  // Cell Voltage (V)
        const float SOC_MEAN = 50.0f,  SOC_SCALE = 29.0f;  // Cell SOC (%)
        const float I_MEAN   = 0.0f,   I_SCALE   = 1.20f;  // Pack Current (A)
        const float T_MEAN   = 35.0f,  T_SCALE   = 10.0f;  // Temperature (°C)

        // Điền 10 features vào input tensor
        for (int i = 0; i < 4; i++) {
            input->data.f[i]     = (cv[i].as<float>()   - V_MEAN)   / V_SCALE;    // V_c1..V_c4
            input->data.f[4 + i] = (csoc[i].as<float>() - SOC_MEAN) / SOC_SCALE;  // SOC_c1..SOC_c4
        }
        input->data.f[8] = (I - I_MEAN) / I_SCALE;   // I_pack
        input->data.f[9] = (T - T_MEAN) / T_SCALE;   // T_pack

        unsigned long ai_start = micros();
        if (interpreter->Invoke() == kTfLiteOk) {
            float max_prob = output->data.f[0];
            pack_ai_class  = 0;
            for (int j = 1; j < 6; j++) {  // 6 classes (0..5)
                if (output->data.f[j] > max_prob) {
                    max_prob      = output->data.f[j];
                    pack_ai_class = j;
                }
            }
            pack_ai_score = max_prob * 100.0f;
        } else {
            Serial.println("-> [AI] Lỗi Invoke TFLite Pack-level!");
        }
        unsigned long ai_end = micros();
        total_inference_time_us = ai_end - ai_start;
    }

    // ============================================================
    // BƯỚC 2: PER-CELL GUARDRAIL (Không dùng AI — chỉ ngưỡng cứng)
    // Xác định cell nào bị lỗi để hiển thị màu trên Dashboard
    // ============================================================
    JsonArray cai_class = doc.createNestedArray("CAI_Class");
    for (int i = 0; i < 4; i++) {
        float cell_v  = cv[i].as<float>();
        int cell_class = 0;

        if      (cell_v > CELL_OV_THRESHOLD)                             cell_class = 1;
        else if (cell_v < CELL_UV_THRESHOLD)                             cell_class = 2;
        else if (abs(I) > PACK_OC_THRESHOLD)                             cell_class = 3;
        else if (T > PACK_OT_THRESHOLD)                                  cell_class = 4;
        else if (cell_imbalance > CELL_IMBALANCE_MAX && cell_v == min_cell_v) cell_class = 5;

        cai_class.add(cell_class);
    }

    // ============================================================
    // BƯỚC 3: PACK GUARDRAIL OVERRIDE (Ưu tiên tuyệt đối)
    // Đảm bảo AI không bao giờ bỏ sót lỗi vật lý nghiêm trọng
    // ============================================================
    if      (max_cell_v > CELL_OV_THRESHOLD) { pack_ai_class = 1; pack_ai_score = 99.5f; }
    else if (min_cell_v < CELL_UV_THRESHOLD) { pack_ai_class = 2; pack_ai_score = 99.5f; }
    else if (abs(I) > PACK_OC_THRESHOLD)     { pack_ai_class = 3; pack_ai_score = 99.5f; }
    else if (T > PACK_OT_THRESHOLD)          { pack_ai_class = 4; pack_ai_score = 99.5f; }
    else if (cell_imbalance > CELL_IMBALANCE_MAX && pack_ai_class == 0) {
        pack_ai_class = 5;
        pack_ai_score = fminf(99.5f, 50.0f + (cell_imbalance - CELL_IMBALANCE_MAX) / CELL_IMBALANCE_MAX * 50.0f);
        Serial.printf("-> [CẢNH BÁO 4S] Độ lệch cell quá lớn: %.3f V!\n", cell_imbalance);
    }

    // Gán dữ liệu AI tổng hợp vào gói JSON cho Web/Digital Twin
    unsigned long total_time_ms = (total_inference_time_us + 500) / 1000;
    if (total_time_ms == 0 && total_inference_time_us > 0) total_time_ms = 1;

    doc["AI_Class"] = pack_ai_class;
    doc["AI_Score"] = pack_ai_score;
    doc["AI_Time"]  = total_time_ms;

    // Đảm bảo mảng CSOH cho 4 Cell có mặt nếu STM32 chưa gửi
    if (!doc.containsKey("CSOH")) {
        JsonArray csoh = doc.createNestedArray("CSOH");
        for (int i = 0; i < 4; i++) {
            csoh.add(pack_soh);
        }
    }

    // Đảm bảo trạng thái (Status) hoạt động
    if (!doc.containsKey("Status")) {
        if (I > 0.01f) doc["Status"] = "Discharging";
        else if (I < -0.01f) doc["Status"] = "Charging";
        else doc["Status"] = "Idle";
    }

    // Xuất gói JSON cập nhật
    String updatedJsonString;
    serializeJson(doc, updatedJsonString);

    // ============================================================
    // BƯỚC 2: IN LOG THÔNG TIN CHI TIẾT RA SERIAL MONITOR
    // ============================================================
    Serial.println("\n----------------- [ESP32 GATEWAY 4S] -----------------");
    Serial.printf("Pack Vol: %.2f V | I: %.3f A | P: %.2f W | T: %.1f C | SOC: %d%% | SOH: %d%%\n",
                  pack_v, I, doc["P"].as<float>(), T, pack_soc, pack_soh);
    Serial.printf("Cell Vol : [C1: %.3fV, C2: %.3fV, C3: %.3fV, C4: %.3fV] (Delta: %.3fV)\n",
                  cv[0].as<float>(), cv[1].as<float>(), cv[2].as<float>(), cv[3].as<float>(), cell_imbalance);
    Serial.printf("Cell SOC : [C1: %d%%, C2: %d%%, C3: %d%%, C4: %d%%]\n",
                  csoc[0].as<int>(), csoc[1].as<int>(), csoc[2].as<int>(), csoc[3].as<int>());
    Serial.printf("AI Result: 4-Cell [%d, %d, %d, %d] -> Pack: %s (Conf: %.1f%%, %lu ms)\n",
                  cai_class[0].as<int>(), cai_class[1].as<int>(), cai_class[2].as<int>(), cai_class[3].as<int>(),
                  AI_CLASS_NAMES[pack_ai_class], pack_ai_score, total_time_ms);
    Serial.println("------------------------------------------------------");

    // ============================================================
    // BƯỚC 3: ĐẨY DỮ LIỆU LÊN VERCEL SERVER QUA HTTPS POST
    // ============================================================
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(secureClient, serverUrl);
        http.addHeader("Content-Type", "application/json");
        http.setReuse(true);   // Tái sử dụng kết nối TLS giúp giảm thời gian handshake
        http.setTimeout(4000); // Timeout 4 giây
        
        int httpResponseCode = http.POST(updatedJsonString);
        
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.printf("-> [HTTP POST] Thành công! Response code: %d\n", httpResponseCode);
            
            // ========================================================
            // BƯỚC 4: NHẬN LỆNH CẤU HÌNH TỪ WEB VÀ GỬI XUỐNG STM32
            // ========================================================
            int cmdIndex = response.indexOf("\"command\":\"");
            if (cmdIndex > 0) {
                int start = cmdIndex + 11;
                int end = response.indexOf("\"", start);
                if (end > start) {
                    String cmd = response.substring(start, end);
                    // Gửi lệnh xuống STM32 qua Serial1 (UART) kèm \r\n
                    Serial1.println(cmd);
                    Serial.println("-> [DOWNLINK] Gửi lệnh cấu hình xuống STM32: " + cmd);
                }
            }
        } else {
            Serial.printf("-> [HTTP POST LỖI] Mã lỗi: %d\n", httpResponseCode);
        }
        http.end();
    } else {
        Serial.println("-> [WIFI] Chưa kết nối, không thể gửi dữ liệu lên Vercel.");
    }
}

void loop() 
{
    // Kiểm tra và thử kết nối lại WiFi định kỳ mỗi 15 giây nếu bị rớt mạng
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastWifiCheck > 15000) {
            lastWifiCheck = currentMillis;
            Serial.println("-> [WIFI] Đang thử kết nối lại...");
            WiFi.reconnect();
        }
    }

    // Đọc dữ liệu từ STM32 qua UART
    bool packetProcessed = false;
    while (Serial1.available()) 
    {
        String line = Serial1.readStringUntil('\n');
        line.trim();

        if (line.length() > 0) 
        {
            // Kiểm tra xem dòng nhận được có chứa gói JSON không
            int jsonIndex = line.indexOf("JSON:");
            if (jsonIndex >= 0) 
            {
                String jsonString = line.substring(jsonIndex + 5);
                jsonString.trim();

                if (jsonString.startsWith("{") && jsonString.endsWith("}")) {
                    processBatteryTelemetry(jsonString);
                    packetProcessed = true;
                    break; // Thoát vòng lặp đọc sau khi đã xử lý xong 1 gói telemetry
                } else {
                    Serial.println("-> [UART] Gói JSON bị cắt xén hoặc sai định dạng: " + jsonString);
                }
            } 
            else if (line.indexOf("Pack Vol:") >= 0 || line.indexOf("Battery") >= 0) 
            {
                // In ra khi nhận được các dòng text debug từ STM32
                Serial.println("-> [STM32 Debug]: " + line);
            }
        }
    }

    if (!packetProcessed && !Serial1.available()) 
    {
        // Thông báo trạng thái mỗi 5 giây nếu chưa nhận được tín hiệu UART từ STM32
        if (millis() - lastWaitingMsg > 5000) {
            lastWaitingMsg = millis();
            Serial.printf("-> [ESP32] Đang chờ UART từ STM32 (Chân RX: GPIO %d, TX: GPIO %d, Baud: %d)...\n", RX_PIN, TX_PIN, UART_BAUD);
        }
    }
}
