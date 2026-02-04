/*
 * Configuration file for Irrigation Controller
 * 
 * Customize these settings for your specific setup
 */

#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// MQTT Broker Configuration
#define MQTT_SERVER "mqtt.example.com"
#define MQTT_PORT 1883
#define MQTT_USER "your_mqtt_user"
#define MQTT_PASSWORD "your_mqtt_password"

// MQTT Topics
#define MQTT_TOPIC_STATUS "irrigation/status"
#define MQTT_TOPIC_COMMAND "irrigation/command"
#define MQTT_TOPIC_SENSOR "irrigation/sensor"
#define MQTT_TOPIC_ALERT "irrigation/alert"

// Pin Configuration (ESP32)
#define PIN_SOIL_MOISTURE 34
#define PIN_DHT_SENSOR 4
#define PIN_PUMP_RELAY 5
#define PIN_LED_STATUS 2

// Alternative Pin Configuration (ESP8266)
// #define PIN_SOIL_MOISTURE A0
// #define PIN_DHT_SENSOR D4
// #define PIN_PUMP_RELAY D5
// #define PIN_LED_STATUS D0

// Sensor Thresholds
#define SOIL_DRY_THRESHOLD 30      // % - Start irrigation below this
#define SOIL_WET_THRESHOLD 70      // % - Stop irrigation above this
#define TEMP_MIN 5                 // °C - Don't irrigate below this
#define TEMP_MAX 45                // °C - Don't irrigate above this

// Timing Configuration (milliseconds)
#define SENSOR_READ_INTERVAL 60000          // 1 minute
#define CLOUD_UPDATE_INTERVAL 300000        // 5 minutes
#define SCHEDULE_CHECK_INTERVAL 60000       // 1 minute
#define PUMP_MAX_RUNTIME 600000             // 10 minutes
#define PUMP_MIN_OFF_TIME 1800000           // 30 minutes between runs

// Schedule Defaults
#define DEFAULT_SCHEDULE_HOUR 6             // 6 AM
#define DEFAULT_SCHEDULE_MINUTE 0
#define DEFAULT_SCHEDULE_DURATION 300       // 5 minutes

// NTP Server Configuration
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 0
#define DAYLIGHT_OFFSET_SEC 3600

// System Settings
#define DEVICE_ID "irrigation-001"
#define FIRMWARE_VERSION "1.0.0"

#endif // CONFIG_H
