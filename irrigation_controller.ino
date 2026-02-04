/*
 * Automated Irrigation Controller
 * 
 * This system manages sensors, water pumps, and scheduling
 * via ESP32/ESP8266 microcontroller with cloud connectivity.
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>

// Pin Definitions
#define SOIL_MOISTURE_PIN 34    // Analog pin for soil moisture sensor
#define DHT_PIN 4               // Digital pin for DHT sensor
#define PUMP_RELAY_PIN 5        // Digital pin for pump relay
#define LED_PIN 2               // Built-in LED for status

// Sensor Configuration
#define DHT_TYPE DHT22          // DHT22 (AM2302) or DHT11
#define SOIL_DRY_THRESHOLD 30   // Percentage below which soil is considered dry
#define SOIL_WET_THRESHOLD 70   // Percentage above which soil is considered wet

// Timing Configuration
#define SENSOR_READ_INTERVAL 60000      // Read sensors every 60 seconds
#define CLOUD_UPDATE_INTERVAL 300000    // Update cloud every 5 minutes
#define SCHEDULE_CHECK_INTERVAL 60000   // Check schedule every minute
#define PUMP_MAX_RUNTIME 600000         // Max pump runtime: 10 minutes

// WiFi Configuration (set your credentials)
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT Configuration (set your broker details)
const char* mqtt_server = "mqtt.example.com";
const int mqtt_port = 1883;
const char* mqtt_user = "your_mqtt_user";
const char* mqtt_password = "your_mqtt_password";
const char* mqtt_topic_status = "irrigation/status";
const char* mqtt_topic_command = "irrigation/command";
const char* mqtt_topic_sensor = "irrigation/sensor";

// NTP Configuration for time synchronization
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

// Objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht(DHT_PIN, DHT_TYPE);

// System State
struct SystemState {
  float soilMoisture;
  float temperature;
  float humidity;
  bool pumpRunning;
  unsigned long pumpStartTime;
  bool autoMode;
  bool manualOverride;
  int scheduleHour;      // Hour to run scheduled irrigation (0-23)
  int scheduleMinute;    // Minute to run scheduled irrigation (0-59)
  int scheduleDuration;  // Duration in seconds
  bool scheduleEnabled;
  bool scheduleRanToday;
} state;

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastCloudUpdate = 0;
unsigned long lastScheduleCheck = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Automated Irrigation Controller ===");
  
  // Initialize pins
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize sensors
  dht.begin();
  
  // Initialize system state
  state.soilMoisture = 0;
  state.temperature = 0;
  state.humidity = 0;
  state.pumpRunning = false;
  state.pumpStartTime = 0;
  state.autoMode = true;
  state.manualOverride = false;
  state.scheduleHour = 6;        // Default: 6 AM
  state.scheduleMinute = 0;
  state.scheduleDuration = 300;   // Default: 5 minutes
  state.scheduleEnabled = true;
  state.scheduleRanToday = false;
  
  // Connect to WiFi
  setupWiFi();
  
  // Configure time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Setup MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  
  Serial.println("System initialized successfully");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Maintain MQTT connection
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  
  // Read sensors periodically
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readSensors();
    lastSensorRead = currentMillis;
  }
  
  // Check schedule periodically
  if (currentMillis - lastScheduleCheck >= SCHEDULE_CHECK_INTERVAL) {
    checkSchedule();
    lastScheduleCheck = currentMillis;
  }
  
  // Auto control logic
  if (state.autoMode && !state.manualOverride) {
    autoControl();
  }
  
  // Safety check: stop pump if running too long
  if (state.pumpRunning && (currentMillis - state.pumpStartTime > PUMP_MAX_RUNTIME)) {
    stopPump("Maximum runtime exceeded");
  }
  
  // Update cloud periodically
  if (currentMillis - lastCloudUpdate >= CLOUD_UPDATE_INTERVAL) {
    publishStatus();
    lastCloudUpdate = currentMillis;
  }
  
  delay(100);
}

void setupWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  if (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "IrrigationController-" + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      mqttClient.subscribe(mqtt_topic_command);
      publishStatus();
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // Parse JSON command
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.println("Failed to parse command");
    return;
  }
  
  // Process commands
  if (doc.containsKey("pump")) {
    String cmd = doc["pump"];
    if (cmd == "on") {
      state.manualOverride = true;
      startPump("Manual command");
    } else if (cmd == "off") {
      state.manualOverride = true;
      stopPump("Manual command");
    }
  }
  
  if (doc.containsKey("mode")) {
    String mode = doc["mode"];
    if (mode == "auto") {
      state.autoMode = true;
      state.manualOverride = false;
      Serial.println("Switched to auto mode");
    } else if (mode == "manual") {
      state.autoMode = false;
      Serial.println("Switched to manual mode");
    }
  }
  
  if (doc.containsKey("schedule")) {
    JsonObject schedule = doc["schedule"];
    if (schedule.containsKey("hour")) {
      state.scheduleHour = schedule["hour"];
    }
    if (schedule.containsKey("minute")) {
      state.scheduleMinute = schedule["minute"];
    }
    if (schedule.containsKey("duration")) {
      state.scheduleDuration = schedule["duration"];
    }
    if (schedule.containsKey("enabled")) {
      state.scheduleEnabled = schedule["enabled"];
    }
    Serial.println("Schedule updated");
  }
  
  publishStatus();
}

void readSensors() {
  // Read soil moisture (convert ADC value to percentage)
  int soilRaw = analogRead(SOIL_MOISTURE_PIN);
  state.soilMoisture = map(soilRaw, 4095, 0, 0, 100); // Invert: wet=high, dry=low
  state.soilMoisture = constrain(state.soilMoisture, 0, 100);
  
  // Read temperature and humidity
  state.temperature = dht.readTemperature();
  state.humidity = dht.readHumidity();
  
  // Check if readings are valid
  if (isnan(state.temperature)) {
    state.temperature = -999;
  }
  if (isnan(state.humidity)) {
    state.humidity = -999;
  }
  
  Serial.println("\n--- Sensor Readings ---");
  Serial.print("Soil Moisture: ");
  Serial.print(state.soilMoisture);
  Serial.println("%");
  Serial.print("Temperature: ");
  Serial.print(state.temperature);
  Serial.println("°C");
  Serial.print("Humidity: ");
  Serial.print(state.humidity);
  Serial.println("%");
  
  // Publish sensor data to cloud
  publishSensorData();
}

void autoControl() {
  // Automatic irrigation based on soil moisture
  if (state.soilMoisture < SOIL_DRY_THRESHOLD && !state.pumpRunning) {
    startPump("Soil too dry (auto mode)");
  } else if (state.soilMoisture > SOIL_WET_THRESHOLD && state.pumpRunning) {
    stopPump("Soil sufficiently wet (auto mode)");
  }
}

void checkSchedule() {
  if (!state.scheduleEnabled) {
    return;
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  
  // Reset daily flag at midnight
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
    state.scheduleRanToday = false;
  }
  
  // Check if it's time to run scheduled irrigation
  if (!state.scheduleRanToday && 
      timeinfo.tm_hour == state.scheduleHour && 
      timeinfo.tm_min == state.scheduleMinute) {
    Serial.println("Running scheduled irrigation");
    startPump("Scheduled irrigation");
    state.scheduleRanToday = true;
    
    // Stop pump after scheduled duration
    // (In real implementation, would use a timer)
  }
}

void startPump(const char* reason) {
  if (!state.pumpRunning) {
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    state.pumpRunning = true;
    state.pumpStartTime = millis();
    
    Serial.print("PUMP STARTED: ");
    Serial.println(reason);
    
    publishStatus();
  }
}

void stopPump(const char* reason) {
  if (state.pumpRunning) {
    digitalWrite(PUMP_RELAY_PIN, LOW);
    state.pumpRunning = false;
    
    Serial.print("PUMP STOPPED: ");
    Serial.println(reason);
    
    publishStatus();
  }
}

void publishStatus() {
  if (!mqttClient.connected()) {
    return;
  }
  
  StaticJsonDocument<512> doc;
  doc["pump_running"] = state.pumpRunning;
  doc["auto_mode"] = state.autoMode;
  doc["manual_override"] = state.manualOverride;
  doc["soil_moisture"] = state.soilMoisture;
  doc["temperature"] = state.temperature;
  doc["humidity"] = state.humidity;
  doc["schedule_hour"] = state.scheduleHour;
  doc["schedule_minute"] = state.scheduleMinute;
  doc["schedule_duration"] = state.scheduleDuration;
  doc["schedule_enabled"] = state.scheduleEnabled;
  doc["uptime"] = millis() / 1000;
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  mqttClient.publish(mqtt_topic_status, buffer);
  Serial.println("Status published to cloud");
}

void publishSensorData() {
  if (!mqttClient.connected()) {
    return;
  }
  
  StaticJsonDocument<256> doc;
  doc["soil_moisture"] = state.soilMoisture;
  doc["temperature"] = state.temperature;
  doc["humidity"] = state.humidity;
  doc["timestamp"] = millis();
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  mqttClient.publish(mqtt_topic_sensor, buffer);
}
