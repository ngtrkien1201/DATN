#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
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

// ================= CẤU HÌNH 3 ĐÈN LED VÀ 1 CÒI BUZZER CẢNH BÁO =================
#define LED_GREEN_PIN    4   // LED Xanh: Trạng thái AN TOÀN (Safe / Normal)
#define LED_YELLOW_PIN   5   // LED Vàng: CẢNH BÁO lệch cell (Warning / Imbalance)
#define LED_RED_PIN      6   // LED Đỏ: BÁO ĐỘNG SỰ CỐ (Fault Alert / Cutoff)
#define BUZZER_PIN       7   // Còi Buzzer: Cảnh báo âm thanh khi xảy ra lỗi

enum AlarmLevel {
    ALARM_OFF,        // Chưa có pin hoặc mất kết nối
    ALARM_SAFE,       // Bình thường: LED Xanh sáng, còi tắt
    ALARM_WARNING,    // Cảnh báo: LED Vàng sáng, còi tắt
    ALARM_FAULT       // Sự cố nguy hiểm: LED Đỏ sáng, còi kêu bíp bíp
};

AlarmLevel currentAlarm = ALARM_OFF;

// Che do trinh dien dong bo toan dien:
// Dieu khien ca 3 LED va Coi ESP32, man hinh TFT LCD va MOSFET PB0 (Quat tai) STM32.
enum DemoMode { DEMO_DISABLED, DEMO_NORMAL, DEMO_IMBALANCE, DEMO_FAULT };
volatile DemoMode demoMode = DEMO_DISABLED;
volatile AlarmLevel telemetryAlarm = ALARM_OFF;
volatile int demoAiClass = 0;

bool applyGatewayCommand(const String &command)
{
    String mode = "";
    if (command.startsWith("DEMO:")) {
        mode = command.substring(5);
    } else if (command.startsWith("DEMO_ALARM:")) {
        mode = command.substring(11);
    } else {
        return false;
    }

    mode.trim();
    mode.toUpperCase();

    if (mode == "SAFE" || mode == "NORMAL") {
        demoMode = DEMO_NORMAL;
        currentAlarm = ALARM_SAFE;
        demoAiClass = 0;
        Serial1.println("MOSFET:1");       // Bat MOSFET PB0 (Quat quay)
        Serial1.println("AI_RES:0,99.0");  // STM32 TFT hien thi NORMAL
    } else if (mode == "WARN" || mode == "WARNING" || mode == "IMBALANCE") {
        demoMode = DEMO_IMBALANCE;
        currentAlarm = ALARM_WARNING;
        demoAiClass = 5;
        Serial1.println("MOSFET:1");       // Lech cell la canh bao -> Cho phep quat quay
        Serial1.println("AI_RES:5,85.0");  // STM32 TFT hien thi IMBALAN
    } else if (mode == "OV" || mode == "OVERVOLTAGE") {
        demoMode = DEMO_FAULT;
        currentAlarm = ALARM_FAULT;
        demoAiClass = 1;
        Serial1.println("MOSFET:0");       // Qua ap: Ngat MOSFET PB0 (Quat dung)
        Serial1.println("AI_RES:1,99.5");  // STM32 TFT hien thi OVER-V
    } else if (mode == "UV" || mode == "UNDERVOLTAGE") {
        demoMode = DEMO_FAULT;
        currentAlarm = ALARM_FAULT;
        demoAiClass = 2;
        Serial1.println("MOSFET:0");       // Sut ap: Ngat MOSFET PB0 (Quat dung)
        Serial1.println("AI_RES:2,99.5");  // STM32 TFT hien thi UNDER-V
    } else if (mode == "OC" || mode == "OVERCURRENT") {
        demoMode = DEMO_FAULT;
        currentAlarm = ALARM_FAULT;
        demoAiClass = 3;
        Serial1.println("MOSFET:0");       // Qua dong: Ngat MOSFET PB0 (Quat dung)
        Serial1.println("AI_RES:3,99.5");  // STM32 TFT hien thi OVER-I
    } else if (mode == "OT" || mode == "OVERHEAT" || mode == "OVERTEMPERATURE") {
        demoMode = DEMO_FAULT;
        currentAlarm = ALARM_FAULT;
        demoAiClass = 4;
        Serial1.println("MOSFET:0");       // Qua nhiet: Ngat MOSFET PB0 (Quat dung)
        Serial1.println("AI_RES:4,99.5");  // STM32 TFT hien thi OVER-T
    } else if (mode == "FAULT") {
        demoMode = DEMO_FAULT;
        currentAlarm = ALARM_FAULT;
        demoAiClass = 1;
        Serial1.println("MOSFET:0");
        Serial1.println("AI_RES:1,99.5");
    } else if (mode == "OFF") {
        demoMode = DEMO_DISABLED;
        currentAlarm = telemetryAlarm;
        demoAiClass = 0;
        Serial1.println("MOSFET:AUTO");    // Thoat demo: Tra lai tu dong cho cam bien STM32
    } else {
        return false;
    }

    Serial.printf("[DEMO SYNC] Da ap dung che do: %s (AI Class: %d)\n", mode.c_str(), demoAiClass);
    return true;
}

// Danh mục phân lớp Anomaly của mô hình AI
const char* AI_CLASS_NAMES[] = {
    "Normal Operation",
    "Over-voltage",
    "Under-voltage",
    "Over-current",
    "Over-heat",
    "Cell Imbalance"   // Class 5: Mất cân bằng áp giữa các cell
};

// Cấu hình ngưỡng bảo vệ vật lý an toàn (Safety Guardrails) mới cho Pack 4S
#define CELL_OV_THRESHOLD   4.20f   // Quá áp Cell (> 4.20V theo chuẩn Li-ion NMC)
#define CELL_UV_THRESHOLD   3.00f   // Sụt áp Cell (< 3.00V ngắt bảo vệ xả sâu)
#define PACK_OC_THRESHOLD   2.00f   // Quá dòng Pack (|I| > 2.0A theo C-rate 1C)
#define PACK_OT_THRESHOLD   50.0f   // Quá nhiệt Pack (T > 50.0°C)
#define CELL_IMBALANCE_MAX  0.20f   // Độ lệch áp tối đa giữa các cell (200mV)

WiFiClientSecure secureClient;
// Bộ đệm dữ liệu Web riêng biệt (Non-blocking)
String latestJsonPayload = "";
bool hasNewDataForWeb = false;

// ============================================================
// LUỒNG CẢNH BÁO ĐỘC LẬP (FREERTOS TASK CHẠY TÁCH BIỆT WIFI)
// ============================================================
void TaskAlarm(void *pvParameters)
{
    (void)pvParameters;
    static unsigned long lastBuzzerTick = 0;
    static bool buzzerToggle = false;
    static AlarmLevel prevAlarm = ALARM_OFF;

    for (;;)
    {
        unsigned long now = millis();

        // Khi có sự thay đổi trạng thái (chuyển chế độ demo): dập tắt còi ngay lập tức và reset timer
        if (currentAlarm != prevAlarm) {
            prevAlarm = currentAlarm;
            digitalWrite(BUZZER_PIN, LOW);
            buzzerToggle = false;
            lastBuzzerTick = now;
        }

        switch (currentAlarm) 
        {
            case ALARM_SAFE:
                // Trạng thái BÌNH THƯỜNG: LED Xanh sáng, còi tắt
                digitalWrite(LED_GREEN_PIN, HIGH);
                digitalWrite(LED_YELLOW_PIN, LOW);
                digitalWrite(LED_RED_PIN, LOW);
                digitalWrite(BUZZER_PIN, LOW);
                buzzerToggle = false;
                break;

            case ALARM_WARNING:
                // Trạng thái CẢNH BÁO (Lệch cell > 200mV): LED Vàng sáng đứng yên
                digitalWrite(LED_GREEN_PIN, LOW);
                digitalWrite(LED_YELLOW_PIN, HIGH);
                digitalWrite(LED_RED_PIN, LOW);
                // Còi bíp nhẹ 1 nhịp ngắn (100ms) định kỳ mỗi 3 giây để báo hiệu
                if (now - lastBuzzerTick >= 3000) {
                    lastBuzzerTick = now;
                    digitalWrite(BUZZER_PIN, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    digitalWrite(BUZZER_PIN, LOW);
                } else {
                    digitalWrite(BUZZER_PIN, LOW);
                }
                break;

            case ALARM_FAULT:
                // Trạng thái SỰ CỐ NGUY HIỂM (Quá áp, Thấp áp, Quá nhiệt, Quá dòng):
                // LED Đỏ sáng đứng yên + Còi kêu dồn dập chu kỳ 200ms ON / 200ms OFF
                digitalWrite(LED_GREEN_PIN, LOW);
                digitalWrite(LED_YELLOW_PIN, LOW);
                digitalWrite(LED_RED_PIN, HIGH);
                if (now - lastBuzzerTick >= 200) {
                    lastBuzzerTick = now;
                    buzzerToggle = !buzzerToggle;
                    digitalWrite(BUZZER_PIN, buzzerToggle ? HIGH : LOW);
                }
                break;

            case ALARM_OFF:
            default:
                // Khi chưa có dữ liệu từ STM32: Tắt tất cả
                digitalWrite(LED_GREEN_PIN, LOW);
                digitalWrite(LED_YELLOW_PIN, LOW);
                digitalWrite(LED_RED_PIN, LOW);
                digitalWrite(BUZZER_PIN, LOW);
                buzzerToggle = false;
                break;
        }

        // Tần số quét 50ms cực nhạy và mượt mà, không bao giờ bị nghẽn bởi WiFi/Web
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
unsigned long lastWifiCheck = 0;
unsigned long lastWaitingMsg = 0;

void setup_wifi() 
{
    Serial.println();
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
}

void setup() 
{
    // Tat bo phat hien sut ap Brownout Detector (ngan chip bi reset khi coi buzzer keu hoac bat WiFi)
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    Serial.begin(115200);
    delay(300); // Khoi dong Serial nhanh gon

    // Mo rong RX Buffer cua Serial1 len 2048 bytes
    Serial1.setRxBufferSize(2048);
    Serial1.begin(UART_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial1.setTimeout(20);

    Serial.println("\n\n==================================================");
    Serial.println("   ESP32 GATEWAY - 4S BATTERY MANAGEMENT SYSTEM   ");
    Serial.println("==================================================");

    Serial.println("Khoi dong Edge AI (TensorFlow Lite Micro)...");
    model = tflite::GetModel(anomaly_model_tflite);

    static tflite::AllOpsResolver micro_op_resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.println("LOI: Khong du bo nho Tensor Arena!");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);
    Serial.println("-> Edge AI Model san sang!");

    secureClient.setInsecure();

    // Khoi tao chan dieu khien 3 LED va Coi Buzzer
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_YELLOW_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    // Ban dau tat tat ca den, trang thai nao thi TaskAlarm se tu bat dung den do
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_YELLOW_PIN, LOW);
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    // KHỞI CHẠY LUỒNG CẢNH BÁO ĐỘC LẬP (Ưu tiên cao nhất Priority 2)
    xTaskCreate(TaskAlarm, "TaskAlarm", 2048, NULL, 2, NULL);

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

    int num_inputs  = (input != nullptr && input->dims != nullptr) ? input->dims->data[1] : 0;
    int num_outputs = (output != nullptr && output->dims != nullptr) ? output->dims->data[1] : 0;

    if (interpreter != nullptr && input != nullptr && output != nullptr && num_inputs > 0) {
        if (num_inputs == 10) {
            // ==============================================================
            // CHUẨN HÓA DỮ LIỆU 4S (10 Features) khi đã train model mới
            // ==============================================================
            const float V_MEAN   = 3.79f,  V_SCALE   = 0.39f;
            const float SOC_MEAN = 54.9f,  SOC_SCALE = 26.8f;
            const float I_MEAN   = 0.08f,  I_SCALE   = 0.85f;
            const float T_MEAN   = 34.4f,  T_SCALE   = 9.1f;

            for (int i = 0; i < 4; i++) {
                input->data.f[i]     = (cv[i].as<float>()   - V_MEAN)   / V_SCALE;
                input->data.f[4 + i] = (csoc[i].as<float>() - SOC_MEAN) / SOC_SCALE;
            }
            input->data.f[8] = (I - I_MEAN) / I_SCALE;
            input->data.f[9] = (T - T_MEAN) / T_SCALE;
        } else if (num_inputs == 4) {
            // ==============================================================
            // MODEL HIỆN TẠI (4 Features: [V_avg, I, T, SOC]) - Tránh tràn RAM tuyệt đối
            // ==============================================================
            float avg_cell_v = pack_v / 4.0f;
            input->data.f[0] = (avg_cell_v - 3.70f) / 0.5f;
            input->data.f[1] = (I - 0.0f) / 1.0f;
            input->data.f[2] = (T - 30.0f) / 15.0f;
            input->data.f[3] = ((float)pack_soc - 50.0f) / 30.0f;
        }

        unsigned long ai_start = micros();
        if (interpreter->Invoke() == kTfLiteOk && num_outputs > 0) {
            float max_prob = output->data.f[0];
            pack_ai_class  = 0;
            for (int j = 1; j < num_outputs; j++) {
                if (output->data.f[j] > max_prob) {
                    max_prob      = output->data.f[j];
                    pack_ai_class = j;
                }
            }
            pack_ai_score = max_prob * 100.0f;
        } else {
            Serial.println("-> [AI] Lỗi Invoke TFLite!");
        }
        unsigned long ai_end = micros();
        total_inference_time_us = ai_end - ai_start;
    }

    // ============================================================
    // BƯỚC 2: PER-CELL GUARDRAIL (Không dùng AI — chỉ ngưỡng cứng)
    // Xác định cell nào bị lỗi để hiển thị màu trên Dashboard
    // ============================================================
    JsonArray cai_class = doc["CAI_Class"].to<JsonArray>();
    for (int i = 0; i < 4; i++) {
        float cell_v  = cv[i].as<float>();
        int cell_class = 0;

        if      (cell_v > CELL_OV_THRESHOLD)                                                cell_class = 1;
        else if (cell_v < CELL_UV_THRESHOLD)                                                cell_class = 2;
        else if (abs(I) > PACK_OC_THRESHOLD)                                                cell_class = 3;
        else if (T > PACK_OT_THRESHOLD)                                                     cell_class = 4;
        else if (cell_imbalance > CELL_IMBALANCE_MAX && pack_soc > 5 && cell_v == min_cell_v) cell_class = 5;

        cai_class.add(cell_class);
    }

    // ============================================================
    // BƯỚC 3: PACK GUARDRAIL OVERRIDE (Ưu tiên an toàn vật lý tuyệt đối)
    // Thu thập toàn bộ cờ lỗi (Fault_Flags) và phân xử nhãn chính theo thứ tự ưu tiên:
    // 1. Over-voltage > 2. Under-voltage > 3. Over-current > 4. Over-temperature > 5. Cell-Imbalance
    // Nếu không vi phạm ngưỡng cứng thì GIỮ NGUYÊN kết quả suy luận từ mô hình AI (TFLite)
    // ============================================================
    String fault_flags = "";
    if (max_cell_v > CELL_OV_THRESHOLD) { fault_flags += (fault_flags.length() > 0 ? ",OV" : "OV"); }
    if (min_cell_v < CELL_UV_THRESHOLD) { fault_flags += (fault_flags.length() > 0 ? ",UV" : "UV"); }
    if (abs(I) > PACK_OC_THRESHOLD)     { fault_flags += (fault_flags.length() > 0 ? ",OC" : "OC"); }
    if (T > PACK_OT_THRESHOLD)          { fault_flags += (fault_flags.length() > 0 ? ",OT" : "OT"); }
    if (cell_imbalance > CELL_IMBALANCE_MAX && pack_soc > 5) {
        fault_flags += (fault_flags.length() > 0 ? ",CI" : "CI");
    }

    // Phân xử nhãn chính: Chỉ override khi phát hiện lỗi vật lý rõ ràng
    if      (max_cell_v > CELL_OV_THRESHOLD) { pack_ai_class = 1; pack_ai_score = 99.5f; }
    else if (min_cell_v < CELL_UV_THRESHOLD) { pack_ai_class = 2; pack_ai_score = 99.5f; }
    else if (abs(I) > PACK_OC_THRESHOLD)     { pack_ai_class = 3; pack_ai_score = 99.5f; }
    else if (T > PACK_OT_THRESHOLD)          { pack_ai_class = 4; pack_ai_score = 99.5f; }
    else if (cell_imbalance > CELL_IMBALANCE_MAX && pack_soc > 5) {
        pack_ai_class = 5;
        pack_ai_score = fminf(99.5f, 75.0f + (cell_imbalance - CELL_IMBALANCE_MAX) * 100.0f);
        Serial.printf("-> [CẢNH BÁO 4S] Lệch cell bất thường: %.3f V (SOC=%d%%)!\n", cell_imbalance, pack_soc);
    }
    else {
        // NẾU KHÔNG VI PHẠM NGƯỠNG CỨNG: Giữ nguyên kết quả suy luận từ Edge AI Model
        // Nếu AI đoán Normal (0) thì điểm tin cậy cao
        if (pack_ai_class == 0 && pack_ai_score < 50.0f) {
            pack_ai_score = 95.0f;
        }
    }

    if (fault_flags.length() == 0) {
        fault_flags = (pack_ai_class == 0) ? "NORMAL" : AI_CLASS_NAMES[pack_ai_class];
    }

    // Gán dữ liệu AI và cờ lỗi tổng hợp vào gói JSON cho Web/Digital Twin
    unsigned long total_time_ms = (total_inference_time_us + 500) / 1000;
    if (total_time_ms == 0 && total_inference_time_us > 0) total_time_ms = 1;

    doc["AI_Class"]    = pack_ai_class;
    doc["AI_Score"]    = pack_ai_score;
    doc["AI_Time"]     = total_time_ms;
    doc["Fault_Flags"] = fault_flags;

    // Đảm bảo mảng CSOH cho 4 Cell có mặt nếu STM32 chưa gửi
    if (!doc.containsKey("CSOH")) {
        JsonArray csoh = doc["CSOH"].to<JsonArray>();
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
    Serial.printf("AI Result: 4-Cell [%d, %d, %d, %d] -> Pack: %s (Flags: %s, Conf: %.1f%%, %lu ms)\n",
                  cai_class[0].as<int>(), cai_class[1].as<int>(), cai_class[2].as<int>(), cai_class[3].as<int>(),
                  AI_CLASS_NAMES[pack_ai_class], fault_flags.c_str(), pack_ai_score, total_time_ms);
    Serial.println("------------------------------------------------------");

    // ============================================================
    // BƯỚC 3: PHẢN HỒI KẾT QUẢ AI TỨC THÌ VỀ STM32 QUA UART (~1.3ms)
    // ============================================================
    // [CAP NHAT CANH BAO TUC THI - 0 DELAY]
    if (pack_ai_class == 1 || pack_ai_class == 2 || pack_ai_class == 3 || pack_ai_class == 4) {
        telemetryAlarm = ALARM_FAULT;    // Su co nguy hiem -> Den Do sang dung yen + Coi keu
    } else if (pack_ai_class == 5 || cell_imbalance > CELL_IMBALANCE_MAX) {
        telemetryAlarm = ALARM_WARNING;  // Lech cell -> Den Vang sang dung yen
    } else {
        telemetryAlarm = ALARM_SAFE;     // Binh thuong -> Den Xanh sang dung yen
    }
    if (demoMode == DEMO_DISABLED) {
        currentAlarm = telemetryAlarm;
    }

    // KICH HOAT NGAY LAP TUC DEN VA COI (KHONG CHO DOI GUI MANG HTTP)
    updateAlarmIndicators();

    if (demoMode == DEMO_DISABLED) {
        Serial1.printf("AI_RES:%d,%.1f\r\n", pack_ai_class, pack_ai_score);
        Serial.printf("-> [UPLINK TO STM32] Phan hoi ket qua AI: AI_RES:%d,%.1f\n", pack_ai_class, pack_ai_score);
    } else {
        // Trong che do Demo: Lien tuc duy tri trang thai MOSFET va AI Class xuong STM32
        Serial1.printf("AI_RES:%d,99.0\r\n", demoAiClass);
        Serial1.printf("MOSFET:%s\r\n", (demoAiClass == 0 || demoAiClass == 5) ? "1" : "0");
    }

    // ============================================================
    // BƯỚC 4: LƯU GÓI JSON CHO LUỒNG GỬI WEB NỀN (NON-BLOCKING)
    // ============================================================
    latestJsonPayload = updatedJsonString;
    hasNewDataForWeb = true;
}

// Hàm tương thích (Hệ thống cảnh báo đã chuyển sang TaskAlarm FreeRTOS độc lập)
void updateAlarmIndicators() {}

void loop() 
{
    // 1. Luồng gửi dữ liệu lên Web Vercel (chạy non-blocking độc lập, chu kỳ 1.5s phản hồi siêu tốc)
    static unsigned long lastHttpPostTick = 0;
    if (WiFi.status() == WL_CONNECTED && hasNewDataForWeb && (millis() - lastHttpPostTick >= 1500)) 
    {
        lastHttpPostTick = millis();
        hasNewDataForWeb = false;

        HTTPClient http;
        http.begin(secureClient, serverUrl);
        http.addHeader("Content-Type", "application/json");
        http.setReuse(true);    // Giữ kết nối SSL Keep-Alive để không phải bắt tay TLS lại từ đầu
        http.setTimeout(3500);  // Timeout 3.5s đảm bảo bắt tay HTTPS Vercel luôn thành công 100%

        int httpResponseCode = http.POST(latestJsonPayload);
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.printf("-> [HTTP POST] Thành công! Response code: %d\n", httpResponseCode);

            // Nhận lệnh cấu hình từ Web (Parser linh hoạt hỗ trợ mọi định dạng JSON có/không khoảng trắng)
            int cmdIndex = response.indexOf("\"command\":");
            if (cmdIndex >= 0) {
                int colonPos = response.indexOf(":", cmdIndex);
                int valStart = response.indexOf("\"", colonPos);
                if (valStart > 0) {
                    int valEnd = response.indexOf("\"", valStart + 1);
                    if (valEnd > valStart) {
                        String cmd = response.substring(valStart + 1, valEnd);
                        cmd.trim();
                        if (cmd.length() > 0) {
                            if (!applyGatewayCommand(cmd)) {
                                Serial1.println(cmd);
                                Serial.println("-> [DOWNLINK] Gửi lệnh cấu hình xuống STM32: " + cmd);
                            }
                        }
                    }
                }
            }
        } else {
            Serial.printf("-> [HTTP POST LỖI] Mã lỗi: %d\n", httpResponseCode);
        }
        http.end();
    }

    // 2. Kiểm tra trạng thái WiFi định kỳ mỗi 15 giây (chỉ in log ngầm, không gọi hàm block reconnect)
    static unsigned long lastWifiLog = 0;
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastWifiLog > 15000) {
            lastWifiLog = millis();
            Serial.println("-> [WIFI] Đang tìm sóng WiFi ngầm (Không ảnh hưởng Còi/Đèn)...");
        }
    }

    // 3. Đọc dữ liệu từ STM32 qua UART
    bool packetProcessed = false;
    while (Serial1.available()) 
    {
        String line = Serial1.readStringUntil('\n');
        line.trim();

        if (line.length() > 0) 
        {
            int jsonIndex = line.indexOf("JSON:");
            if (jsonIndex >= 0) 
            {
                String jsonString = line.substring(jsonIndex + 5);
                jsonString.trim();

                if (jsonString.startsWith("{") && jsonString.endsWith("}")) {
                    processBatteryTelemetry(jsonString);
                    packetProcessed = true;
                    break;
                } else {
                    Serial.println("-> [UART] Gói JSON bị cắt xén: " + jsonString);
                }
            } 
            else if (line.indexOf("Pack Vol:") >= 0 || line.indexOf("Battery") >= 0 || line.indexOf("I2C") >= 0 || line.indexOf("INA") >= 0) 
            {
                Serial.println("-> [STM32 Debug]: " + line);
            }
        }
    }

    if (!packetProcessed && !Serial1.available()) 
    {
        if (millis() - lastWaitingMsg > 5000) {
            lastWaitingMsg = millis();
            Serial.printf("-> [ESP32] Đang chờ UART từ STM32 (RX: GPIO %d, TX: GPIO %d)...\n", RX_PIN, TX_PIN);
        }
    }
}
