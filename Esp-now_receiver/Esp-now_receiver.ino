#include <ESP8266WiFi.h>
#include <espnow.h>

extern "C" {
  #include "user_interface.h"
}

uint8_t customMac[] = {0x5C, 0xCF, 0x7F, 0x4C, 0x89, 0x5F};

const int ESPNOW_CHANNEL = 1;

int throttle = 0;
int throttle_h = 0;
int steer = 0;
int steer_v = 0;
int btn4_state = 0;
int btn5_state = 0;

unsigned long lastReceiveTime = 0;
const unsigned long FAILSAFE_TIMEOUT = 1000;

void stopMotors() {
  // Put motor stop code here
}

void handleMotorControl() {
  // Put motor control code here
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  lastReceiveTime = millis();

  char csvData[32];

  if (len >= sizeof(csvData)) {
    len = sizeof(csvData) - 1;
  }

  memcpy(csvData, incomingData, len);
  csvData[len] = '\0';

  int parsed = sscanf(
    csvData,
    "%d,%d,%d,%d,%d,%d",
    &throttle,
    &throttle_h,
    &steer,
    &steer_v,
    &btn4_state,
    &btn5_state
  );

  if (parsed == 6) {
    Serial.println(csvData);
    handleMotorControl();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.disconnect();

  wifi_set_macaddr(STATION_IF, customMac);
  wifi_set_channel(ESPNOW_CHANNEL);

  if (esp_now_init() != 0) {
    Serial.println("ERROR: esp_now_init() failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  lastReceiveTime = millis();

  Serial.println("ESP-NOW receiver ready");
  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  if (millis() - lastReceiveTime > FAILSAFE_TIMEOUT) {
    if (
      throttle != 0 ||
      throttle_h != 0 ||
      steer != 0 ||
      steer_v != 0 ||
      btn4_state != 0 ||
      btn5_state != 0
    ) {
      throttle = 0;
      throttle_h = 0;
      steer = 0;
      steer_v = 0;
      btn4_state = 0;
      btn5_state = 0;

     // Serial.println("0,0,0,0,0,0");
      stopMotors();
    }
  }

  delay(10);
}