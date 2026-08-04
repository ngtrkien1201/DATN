#include <WiFi.h>
#include <HTTPClient.h>

// ================= CẤU HÌNH WIFI =================
const char* ssid = "kien";         // Thay tên WiFi của bạn vào đây
const char* password = "12071999"; // Thay mật khẩu WiFi vào đây

// ================= CẤU HÌNH SERVER VERCEL =================
// ĐIỀN ĐƯỜNG DẪN VERCEL CỦA BẠN VÀO ĐÂY (VÍ DỤ: https://my-app.vercel.app/api/telemetry)
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
    // Bật cổng Serial mặc định (Cắm cáp USB vào máy tính để xem Log)
    Serial.begin(115200);
    
    // Bật cổng Serial1 để giao tiếp với STM32 (Baudrate 115200)
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    
    setup_wifi();
}

void loop() 
{
    // Cần giữ kết nối WiFi
    if(WiFi.status() != WL_CONNECTED) {
        setup_wifi();
    }

    // =============== ĐỌC DỮ LIỆU TỪ STM32 ===============
    if (Serial1.available()) 
    {
        String incomingData = Serial1.readStringUntil('\n'); // Đọc từng dòng
        incomingData.trim(); // Cắt ký tự rác \r\n ở cuối
        
        if (incomingData.length() > 0)
        {
            Serial.println("STM32: " + incomingData);

            // Chuỗi từ STM32 gửi lên có dạng: JSON: {"V":4.2, ...}
            int jsonIndex = incomingData.indexOf("JSON:");
            if (jsonIndex >= 0) 
            {
                String jsonString = incomingData.substring(jsonIndex + 5);
                jsonString.trim();
                
                // Kiểm tra JSON hợp lệ: phải bắt đầu bằng '{' và kết thúc bằng '}'
                if (!jsonString.startsWith("{") || !jsonString.endsWith("}")) {
                    Serial.println("-> JSON invalid, skipping: " + jsonString);
                    return;
                }
                
                // Đẩy gói JSON lên Vercel qua HTTP POST
                if(WiFi.status() == WL_CONNECTED){
                    HTTPClient http;
                    http.begin(serverUrl);
                    http.addHeader("Content-Type", "application/json");
                    
                    int httpResponseCode = http.POST(jsonString);
                    
                    if(httpResponseCode > 0){
                        Serial.print("-> HTTP Response code: ");
                        Serial.println(httpResponseCode);
                        
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
                        Serial.print("-> Error code: ");
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
