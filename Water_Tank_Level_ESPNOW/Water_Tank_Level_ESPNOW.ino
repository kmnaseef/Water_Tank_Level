#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

// --- Hardware Pins ---
#define TRIG_PIN 4
#define ECHO_PIN 5
#define MOS_PIN 12
#define BATTERY_PIN A0

// --- Network & OTA Configuration ---
const char* ssid = "🏠";
const char* password = "nsaheec094";
const char* hostName = "watertank";

// --- Receiver MAC Address (Receiver 2 ONLY) ---
uint8_t receiverAddress[] = { 0xE8, 0x06, 0x90, 0x98, 0xAD, 0xCC }; 

// Web Server & OTA Updater
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// --- Data Structure for ESP-NOW ---
typedef struct SensorData {
  float distance;
  float voltage;
  bool isConnected;
} SensorData;

SensorData myData;

// --- RTC Memory Structure ---
#define RTC_MAGIC 0x55AA55AA

struct RtcData {
  uint32_t magic;
  uint32_t bootCount;
} rtcData;

// --- State Variables & Timers ---
bool keepAwake = false;
unsigned long lastMeasurement = 0;

const int SLEEP_INTERVAL_SECONDS = 5;
const int BOOTS_PER_15_MINS = (1 * 60) / SLEEP_INTERVAL_SECONDS;

// --- Function Declarations ---
float measureDistance();
float readBattery();
void powerSensor(bool state);

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Booting ---");

  // 1. Initialize Power Control
  pinMode(MOS_PIN, OUTPUT);
  powerSensor(true);

  // 2. Initialize Sensor Pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // 3. Read RTC Memory for Boot Tracking
  ESP.rtcUserMemoryRead(0, (uint32_t*)&rtcData, sizeof(rtcData));

  if (rtcData.magic != RTC_MAGIC) {
    rtcData.magic = RTC_MAGIC;
    rtcData.bootCount = BOOTS_PER_15_MINS;  // Force first boot WiFi check
  } else {
    rtcData.bootCount++;
  }

  // 4. Measure Distance & Battery
  myData.distance = measureDistance();
  myData.voltage = readBattery();
  myData.isConnected = false;

  // Turn OFF sensor to save power
  powerSensor(false);

  // 5. Setup ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

    // Add peer and send initial data to Receiver 2
    esp_now_add_peer(receiverAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
    esp_now_send(receiverAddress, (uint8_t*)&myData, sizeof(myData));

    Serial.printf("ESP-NOW Sent to receiver - Dist: %.2f cm, Bat: %.2fV\n", myData.distance, myData.voltage);

  } else {
    Serial.println("ESP-NOW Init Failed");
  }

  // 6. Check if it's time to scan WiFi
  if (rtcData.bootCount >= BOOTS_PER_15_MINS) {

    rtcData.bootCount = 0;

    Serial.println("Attempting WiFi Connection...");
    WiFi.hostname("WATERTANK");
    WiFi.begin(ssid, password);
    int retries = 0;

    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Connected! Staying awake until AP goes offline.");
      // CRITICAL: Print the Wi-Fi channel the router assigned us
      Serial.printf("Connected on Wi-Fi Channel: %d\n", WiFi.channel());
      keepAwake = true;

      if (MDNS.begin(hostName)) {
        Serial.println("mDNS responder started: http://watertank.local");
      }

      httpUpdater.setup(&httpServer);
      httpServer.begin();
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("\nWiFi Failed. Going to sleep.");
      WiFi.disconnect();
    }
  }

  // Save updated RTC Data
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtcData, sizeof(rtcData));

  // 7. Sleep Logic (if not keeping awake)
  if (!keepAwake) {
    Serial.println("Entering Deep Sleep for 5 seconds...");
    ESP.deepSleep(SLEEP_INTERVAL_SECONDS * 1000000);
  }
}

void loop() {

  if (keepAwake) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Access Point lost. Returning to Deep Sleep.");
      WiFi.disconnect();
      ESP.deepSleep(SLEEP_INTERVAL_SECONDS * 1000000);
    }

    httpServer.handleClient();
    MDNS.update();

    if (millis() - lastMeasurement >= (SLEEP_INTERVAL_SECONDS * 1000)) {
      lastMeasurement = millis();
      powerSensor(true);
      myData.distance = measureDistance();
      powerSensor(false);
      myData.voltage = readBattery();
      myData.isConnected = true;

      // Send to the single receiver while on WiFi
      esp_now_send(receiverAddress, (uint8_t*)&myData, sizeof(myData));
      
      Serial.printf("Awake ESP-NOW Sent - Dist: %.2f cm, Bat: %.2fV, WiFi: %d\n", myData.distance, myData.voltage, myData.isConnected);
    }
  }
}

// --- Helper Functions ---

float readBattery() {
  int rawBattery = analogRead(BATTERY_PIN);
  return (4.24 * rawBattery) / 1023.0;
}

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;
  return (duration * 0.0343) / 2;
}

void powerSensor(bool state) {
  if (state) {
    digitalWrite(MOS_PIN, LOW);
    delay(50);
  } else {
    digitalWrite(MOS_PIN, HIGH);
  }
}