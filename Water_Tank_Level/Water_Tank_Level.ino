#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>

// Your Wi-Fi Credentials
const char* ssid = "🏠";
const char* password = "nsaheec094";

// Pin definitions for AJ-SR04M
const int trigPin = 5; // D1 on ESP8266
const int echoPin = 4; // D2 on ESP8266

// --- CALIBRATE YOUR TANK HERE ---
// Distance from the face of the sensor to the BOTTOM of the empty tank (in cm)
const float TANK_DEPTH = 100.0; 
// Distance from the sensor to the MAXIMUM water level (in cm) 
const float SENSOR_DEAD_ZONE = 20.0; 

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Connect to Wi-Fi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Start the mDNS responder for http://watertank.local
  if (MDNS.begin("watertank")) {
    Serial.println("MDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  // 1. Set up the main web server route
  server.on("/", handleRoot);

  // 2. Attach the OTA update server to the web server
  // This automatically creates the "/update" route
  httpUpdater.setup(&server);

  // Start the server
  server.begin();
  
  // Announce the web server via mDNS
  MDNS.addService("http", "tcp", 80);
  
  Serial.println("HTTP server started");
  Serial.println("Dashboard ready at: http://watertank.local");
  Serial.println("Update page ready at: http://watertank.local/update");
}

void loop() {
  // Listen for HTTP requests (both dashboard and OTA)
  server.handleClient();
  
  // Keep mDNS broadcast alive
  MDNS.update();
}

void handleRoot() {
  // 1. Trigger the ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // 2. Read the echo (with a 30ms timeout so a disconnected sensor doesn't crash the server)
  long duration = pulseIn(echoPin, HIGH, 30000); 
  
  // 3. Calculate distance in cm
  float distance = (duration * 0.0343) / 2;

  // 4. Calculate water level and percentage
  float waterLevel = TANK_DEPTH - distance;
  
  // Keep values within logical bounds
  if (waterLevel < 0 || duration == 0) waterLevel = 0; // duration 0 means timeout/no reading
  if (waterLevel > TANK_DEPTH) waterLevel = TANK_DEPTH;

  // Calculate percentage (factoring in the dead zone at the top)
  float usableDepth = TANK_DEPTH - SENSOR_DEAD_ZONE;
  float percentage = (waterLevel / usableDepth) * 100.0;
  if (percentage > 100.0) percentage = 100.0;

  // 5. Build the HTML Web Page
  String html = "<!DOCTYPE html><html><head><title>Water Tank Level</title>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; margin-top: 50px; background-color: #f4f4f9; color: #333;}";
  html += ".container { background: white; padding: 30px; border-radius: 15px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); display: inline-block; width: 80%; max-width: 400px; }";
  html += ".meter { width: 100%; background-color: #e0e0e0; border-radius: 25px; margin: 20px auto; height: 40px; overflow: hidden; }";
  html += ".fill { height: 100%; background-color: #3498db; transition: width 0.5s ease-in-out; }";
  html += "h1 { color: #2c3e50; }";
  html += "</style></head><body>";
  
  html += "<div class=\"container\">";
  html += "<h1>Tank Status</h1>";
  html += "<p>Distance to water: <strong>" + String(distance, 1) + " cm</strong></p>";
  html += "<p>Actual Water Level: <strong>" + String(waterLevel, 1) + " cm</strong></p>";
  html += "<h2>" + String(percentage, 1) + "% Full</h2>";
  html += "<div class=\"meter\"><div class=\"fill\" style=\"width:" + String(percentage) + "%\"></div></div>";
  html += "<p style='font-size: 0.8em; color: #777;'>Updates automatically every 5 seconds</p>";
  
  // Provide a handy link to the update page using the .local address
  html += "<br><a href=\"/update\" style=\"font-size: 0.8em; color: #3498db; text-decoration: none;\">Firmware Update Page</a>";
  html += "</div>";
  
  // Auto-refresh the page every 5 seconds
  html += "<script>setTimeout(function(){location.reload()}, 5000);</script>"; 
  html += "</body></html>";

  // Send the page to the browser
  server.send(200, "text/html", html);
}