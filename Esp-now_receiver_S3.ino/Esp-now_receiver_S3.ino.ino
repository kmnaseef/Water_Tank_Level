#include <WiFi.h>
#include <esp_now.h>

// --- Data Structure for ESP-NOW ---
// IMPORTANT: This MUST perfectly match the transmitter's struct!
typedef struct SensorData {
  float distance;
  float voltage;
  bool isConnected;
} SensorData;

SensorData incomingData;

// --- Callback Function ---
// This runs automatically in the background whenever data arrives
// Note: ESP32 uses esp_now_recv_info_t instead of a direct MAC address array
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingDataPtr, int len) {
  // Security/Safety check: Ensure the incoming packet is exactly the size we expect
  if (len == sizeof(SensorData)) {
    // Copy the raw bytes into our structured variable
    memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
    
    Serial.println("\n--- New Data Received ---");
    Serial.printf("Distance   : %.2f cm\n", incomingData.distance);
    Serial.printf("Battery    : %.2f V\n", incomingData.voltage);
    
    Serial.print("Transmitter: ");
    if (incomingData.isConnected) {
      Serial.println("AWAKE (AP is Online)");
    } else {
      Serial.println("SLEEPING (AP is Offline)");
    }
    Serial.println("-------------------------");
    
  } else {
    Serial.printf("\nError: Received %d bytes, expected %d bytes\n", len, sizeof(SensorData));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Receiver Booting (ESP32-S3) ---");

  // 1. Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Must disconnect from traditional WiFi for pure ESP-NOW

  // 2. Print MAC Address
  // VERIFY THIS MATCHES THE 'receiverAddress' IN YOUR TRANSMITTER CODE!
  Serial.print("My MAC Address: ");
  Serial.println(WiFi.macAddress());

  // 3. Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // 4. Register the receive callback function
  // (ESP32 does not require setting a "Slave" role like the ESP8266 did)
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println("Setup complete. Waiting for data...");
}

void loop() {
  // The loop stays completely empty!
  // The OnDataRecv callback handles everything asynchronously in the background
  // whenever a packet flies through the air.
}