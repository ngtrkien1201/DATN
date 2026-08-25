#include <WiFi.h>
#include <HTTPClient.h>
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

constexpr int kTensorArenaSize = 8192;
uint8_t tensor_arena[kTensorArenaSize];
// ============================================================

// ================= CẤU HÌNH WIFI =================
const char* ssid = "LinhTrung_3";         // Thay tên WiFi của bạn vào đây
const char* password = "23012024"; // Thay mật khẩu WiFi vào đây

// ================= CẤU HÌNH SERVER VERCEL =================
const char* serverUrl = "https://datn-mauve.vercel.app/api/telemetry";

// ================= CẤU HÌNH UART =================
#define RX_PIN 16
#define TX_PIN 17

void setup_wifi() 
{
    delay(10);
    Serial.println();
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup() 
{
    Serial.begin(115200);
    delay(3000); // Chờ cổng USB Serial kết nối với máy tính
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    
    Serial.println("\n\n--- HỆ THỐNG ESP32 KHỞI ĐỘNG ---");
    Serial.println("Khởi động AI (TensorFlow Lite)...");
    // Khởi tạo model từ mảng byte C++
    model = tflite::GetModel(anomaly_model_tflite);
    
    // Đăng ký các phép toán
    static tflite::AllOpsResolver micro_op_resolver;
    
    // Tạo Interpreter
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;
    
    // Cấp phát bộ nhớ
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.println("LỖI: Không đủ bộ nhớ Tensor Arena! Hãy tăng kTensorArenaSize.");
        return;
    }
    
    // Gán con trỏ đầu vào/đầu ra
    input = interpreter->input(0);
    output = interpreter->output(0);
    
    if (input == nullptr || output == nullptr) {
        Serial.println("LỖI: Không lấy được con trỏ Input/Output của Model!");
        return;
    }
    
    Serial.println("AI Sẵn sàng!");

    setup_wifi();
}

void loop() 
{
    if(WiFi.status() != WL_CONNECTED) {
        setup_wifi();
    }

    if (Serial1.available()) 
    {
        String incomingData = "";
        while (Serial1.available()) {
            String line = Serial1.readStringUntil('\n');
            line.trim();
            if (line.indexOf("JSON:") >= 0) {
                incomingData = line;
            }
        }
        
        if (incomingData.length() > 0)
        {
            Serial.println("STM32: " + incomingData);

            int jsonIndex = incomingData.indexOf("JSON:");
            if (jsonIndex >= 0) 
            {
                String jsonString = incomingData.substring(jsonIndex + 5);
                jsonString.trim();
                
                if (!jsonString.startsWith("{") || !jsonString.endsWith("}")) {
                    Serial.println("-> JSON invalid, skipping: " + jsonString);
                    return;
                }

                // ============================================================
                // BƯỚC 1: TRÍCH XUẤT DATA & CHẠY AI INFERENCE TRÊN MẠCH ESP32
                // ============================================================
                #if ARDUINOJSON_VERSION_MAJOR >= 7
                    JsonDocument doc;
                #else
                    DynamicJsonDocument doc(512);
                #endif
                
                DeserializationError error = deserializeJson(doc, jsonString);
                
                if (!error) {
                    float V = doc["V"];
                    float I = doc["I"];
                    float T = doc["T"];
                    float SOC = doc["SOC"];

                    // Chuẩn hóa dữ liệu (Scaler) Y HỆT như trên Google Colab
                    float V_norm = (V - 3.82491198f) / 0.398182204f;
                    float I_norm = (I - 0.0159294559f) / 0.0118215338f;
                    float T_norm = (T - 35.0664208f) / 5.14145397f;
                    float SOC_norm = (SOC - 68.2045365f) / 23.7133269f;

                    // --- CHẠY INFERENCE TFLITE ---
                    unsigned long ai_start = millis();
                    // 1. Gán giá trị vào mảng input của TensorFlow
                    input->data.f[0] = V_norm;
                    input->data.f[1] = I_norm;
                    input->data.f[2] = T_norm;
                    input->data.f[3] = SOC_norm;
                    
                    // 2. Chạy model
                    if (interpreter->Invoke() != kTfLiteOk) {
                        Serial.println("Lỗi Invoke TFLite!");
                    }
                    
                    // 3. Đọc mảng đầu ra (5 xác suất)
                    float y_pred[5];
                    for (int i = 0; i < 5; i++) {
                        y_pred[i] = output->data.f[i];
                    }
                    
                    unsigned long ai_end = millis();
                    // -----------------------------
                    unsigned long inference_time = ai_end - ai_start;

                    // Tìm Class có tỷ lệ tin cậy (Confidence) cao nhất
                    int ai_class = 0;
                    float max_prob = y_pred[0];
                    for(int i = 1; i < 5; i++) {
                        if(y_pred[i] > max_prob) {
                            max_prob = y_pred[i];
                            ai_class = i;
                        }
                    }

                    // Đóng gói thêm kết quả AI vào chuỗi JSON trước khi gửi lên Vercel
                    doc["AI_Class"] = ai_class;
                    doc["AI_Score"] = max_prob;
                    doc["AI_Time"] = inference_time;

                    String updatedJsonString;
                    serializeJson(doc, updatedJsonString);
                    jsonString = updatedJsonString; // Ghi đè chuỗi json để gửi đi
                    
                    Serial.print("-> [AI ESP32] Du doan: Class ");
                    Serial.print(ai_class);
                    Serial.print(" (Ty le: ");
                    Serial.print(max_prob * 100);
                    Serial.print("%) - Thoi gian: ");
                    Serial.print(inference_time);
                    Serial.println(" ms");
                } else {
                    Serial.println("-> [ERROR] Khong the Parse JSON de chay AI");
                }
                // ============================================================
                
                // Đẩy gói JSON (đã kẹp kết quả AI) lên Vercel qua HTTP POST
                if(WiFi.status() == WL_CONNECTED){
                    HTTPClient http;
                    http.begin(serverUrl);
                    http.addHeader("Content-Type", "application/json");
                    
                    int httpResponseCode = http.POST(jsonString);
                    
                    if(httpResponseCode > 0){
                        // Đọc nội dung phản hồi từ Vercel
                        String response = http.getString();
                        
                        // Tìm xem Vercel có gửi lệnh cấu hình xuống không
                        int cmdIndex = response.indexOf("\"command\":\"");
                        if(cmdIndex > 0)
                        {
                            int start = cmdIndex + 11;
                            int end = response.indexOf("\"", start);
                            if(end > start)
                            {
                                String cmd = response.substring(start, end);
                                // Gửi lệnh xuống STM32 qua Serial1 (UART) kèm \n (println)
                                Serial1.println(cmd); 
                                Serial.println("-> Sent command to STM32: " + cmd);
                            }
                        }
                    } else {
                        Serial.print("-> Error code HTTP: ");
                        Serial.println(httpResponseCode);
                    }
                    http.end();
                } else {
                    Serial.println("-> WiFi Disconnected, cannot send HTTP POST");
                }
            }
        }
    }
}
