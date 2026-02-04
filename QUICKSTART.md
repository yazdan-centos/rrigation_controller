# Quick Start Guide

Get your automated irrigation system up and running in 15 minutes!

## Prerequisites

- ESP32 or ESP8266 development board
- Soil moisture sensor
- DHT22 temperature/humidity sensor
- 5V relay module
- Water pump (5V or 12V)
- USB cable for ESP32/ESP8266
- Jumper wires

## Step 1: Hardware Setup (5 minutes)

### Connect the Components

**ESP32 Connections:**
```
ESP32 Pin    →  Component
GPIO34       →  Soil Moisture Sensor (Signal)
GPIO4        →  DHT22 Sensor (Data)
GPIO5        →  Relay Module (IN)
3.3V         →  DHT22 (VCC)
5V           →  Soil Sensor (VCC), Relay (VCC)
GND          →  All GND pins
```

**Power Supply:**
- ESP32: 5V via USB
- Pump: 12V separate power supply through relay

See [HARDWARE.md](HARDWARE.md) for detailed wiring diagrams.

## Step 2: Software Setup (5 minutes)

### Install Arduino IDE

1. Download from https://www.arduino.cc/en/software
2. Install for your operating system

### Install ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. In "Additional Board Manager URLs" add:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
4. Go to **Tools → Board → Boards Manager**
5. Search for "esp32" and click **Install**

### Install Required Libraries

1. Go to **Tools → Manage Libraries**
2. Search and install:
   - **PubSubClient** (for MQTT)
   - **DHT sensor library** by Adafruit
   - **ArduinoJson** (version 6.x)

## Step 3: Configure (3 minutes)

### Edit config.h

1. Open `config.h` in Arduino IDE
2. Update your WiFi credentials:
   ```cpp
   #define WIFI_SSID "YourWiFiName"
   #define WIFI_PASSWORD "YourWiFiPassword"
   ```

3. Update MQTT broker settings (or use a test broker):
   ```cpp
   #define MQTT_SERVER "test.mosquitto.org"
   #define MQTT_PORT 1883
   #define MQTT_USER ""
   #define MQTT_PASSWORD ""
   ```

### Optional: Adjust Thresholds

```cpp
#define SOIL_DRY_THRESHOLD 30  // Start watering below 30%
#define SOIL_WET_THRESHOLD 70  // Stop watering above 70%
```

## Step 4: Upload (2 minutes)

1. Connect ESP32 to computer via USB
2. In Arduino IDE:
   - Select **Tools → Board → ESP32 Dev Module**
   - Select **Tools → Port → [Your ESP32 Port]**
3. Open `irrigation_controller.ino`
4. Click **Upload** button (→)
5. Wait for "Done uploading" message

## Step 5: Test and Monitor

### Open Serial Monitor

1. Click **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. You should see:
   ```
   === Automated Irrigation Controller ===
   Connecting to WiFi...
   WiFi connected
   IP address: 192.168.x.x
   ```

### Check Sensor Readings

Every minute you'll see:
```
--- Sensor Readings ---
Soil Moisture: 45.2%
Temperature: 22.5°C
Humidity: 65.3%
```

### Test Pump Control

The system will automatically:
- Start pump when soil moisture < 30%
- Stop pump when soil moisture > 70%
- Run scheduled irrigation at 6:00 AM daily

## Quick Commands via MQTT

Use any MQTT client (MQTT Explorer, mosquitto_pub, etc.):

### Start Pump Manually
```bash
mosquitto_pub -h test.mosquitto.org -t irrigation/command -m '{"pump":"on"}'
```

### Stop Pump
```bash
mosquitto_pub -h test.mosquitto.org -t irrigation/command -m '{"pump":"off"}'
```

### Switch to Manual Mode
```bash
mosquitto_pub -h test.mosquitto.org -t irrigation/command -m '{"mode":"manual"}'
```

### Subscribe to Status Updates
```bash
mosquitto_sub -h test.mosquitto.org -t irrigation/#
```

## Default Settings

| Setting | Value |
|---------|-------|
| Dry Threshold | 30% |
| Wet Threshold | 70% |
| Max Runtime | 10 minutes |
| Min Off Time | 30 minutes |
| Schedule | 6:00 AM daily |
| Schedule Duration | 5 minutes |

## Troubleshooting

### WiFi Not Connecting
- Check SSID and password in config.h
- Ensure 2.4GHz network (ESP32/ESP8266 don't support 5GHz)
- Move closer to router

### Sensors Not Reading
- Check wiring connections
- Verify 3.3V/5V power to sensors
- Try different GPIO pins

### Pump Not Starting
- Check relay wiring
- Verify relay module gets power
- Check if safety limits are triggered (view Serial Monitor)

### No MQTT Connection
- Test broker with external MQTT client first
- Check firewall settings
- Try public broker: test.mosquitto.org

## Next Steps

1. **Calibrate Sensors**: See [README.md](README.md#calibration)
2. **Set Up Cloud Service**: Run Python cloud service for data logging
3. **Create Custom Schedules**: Modify schedule settings
4. **Add Multiple Zones**: Connect more pumps and relays
5. **Integrate with Home Automation**: See [API.md](API.md)

## Support

- Full documentation: [README.md](README.md)
- Hardware guide: [HARDWARE.md](HARDWARE.md)
- API reference: [API.md](API.md)
- Issues: Open an issue on GitHub

## Safety Reminders

⚠️ **Important Safety Notes:**
- Never run pump without water (dry running damages pump)
- Use proper electrical isolation for outdoor installations
- Protect electronics from water with IP65 enclosure
- Use separate power supply for pump (don't power from ESP32)
- Add fuse protection for pump circuit
- Follow local electrical codes

---

**That's it! Your irrigation system is now running!** 🎉

The system will now automatically monitor soil moisture and water your plants when needed.
