# Automated Irrigation System

A comprehensive automated irrigation system that manages sensors, water pumps, and scheduling via microcontroller with cloud connectivity.

## Features

### Sensor Management
- **Soil Moisture Sensor**: Real-time soil moisture monitoring with calibration support
- **Temperature & Humidity Sensor**: DHT22/DHT11 support for environmental monitoring
- **Sensor Health Monitoring**: Automatic detection of sensor failures
- **Historical Data**: Tracks sensor readings over time with trend analysis

### Pump Control
- **Smart Pump Control**: Automatic pump activation based on soil moisture levels
- **Safety Features**:
  - Maximum runtime protection (default: 10 minutes)
  - Minimum off-time between runs (default: 30 minutes)
  - Daily runtime limits (default: 1 hour per day)
  - Emergency stop capability
- **Multi-Zone Support**: Control multiple irrigation zones independently
- **Water Usage Tracking**: Estimates water volume used per irrigation cycle

### Scheduling System
- **Flexible Scheduling**: Create multiple schedule rules with different times
- **Day-of-Week Selection**: Run schedules on specific days
- **Duration Control**: Set irrigation duration for each schedule
- **Enable/Disable Rules**: Easily activate or deactivate schedules
- **Automatic Execution**: Schedules run automatically at specified times

### Cloud Connectivity
- **MQTT Protocol**: Real-time communication with cloud services
- **Data Logging**: Stores sensor readings and system events in database
- **Remote Control**: Control pump and settings from anywhere
- **Status Monitoring**: Real-time system status updates
- **Alert System**: Notifications for important events

### Microcontroller Support
- **ESP32/ESP8266**: Primary platform with WiFi connectivity
- **Arduino Compatible**: Can be adapted for Arduino with WiFi/Ethernet shields
- **Low Power Mode**: Optimized for battery-powered deployments

## Hardware Requirements

### Microcontroller
- ESP32 or ESP8266 development board
- Alternative: Arduino Uno/Mega with WiFi/Ethernet shield

### Sensors
1. **Soil Moisture Sensor**
   - Capacitive or resistive soil moisture sensor
   - Connected to analog input (ESP32: GPIO34, ESP8266: A0)
   
2. **DHT Temperature & Humidity Sensor**
   - DHT22 (recommended) or DHT11
   - Connected to digital input (ESP32: GPIO4, ESP8266: D4)

### Actuators
1. **Water Pump**
   - 5V/12V DC water pump or solenoid valve
   - Connected via relay module
   
2. **Relay Module**
   - 5V relay module (1 channel minimum, 4 channels for multi-zone)
   - Connected to digital output (ESP32: GPIO5, ESP8266: D5)

### Power Supply
- 5V USB power supply for ESP32/ESP8266
- Separate power supply for pump (typically 12V DC)

### Optional Components
- Enclosure for weather protection
- Status LED (built-in LED used by default)
- Push button for manual control
- LCD display for local status monitoring

## Pin Configuration

### ESP32 Default Pins
```
Soil Moisture: GPIO34 (Analog)
DHT Sensor:    GPIO4  (Digital)
Pump Relay:    GPIO5  (Digital)
Status LED:    GPIO2  (Built-in)
```

### ESP8266 Default Pins
```
Soil Moisture: A0  (Analog)
DHT Sensor:    D4  (Digital)
Pump Relay:    D5  (Digital)
Status LED:    D0  (Built-in)
```

## Software Installation

### Arduino IDE Setup

1. **Install Arduino IDE**
   - Download from https://www.arduino.cc/en/software

2. **Add ESP32/ESP8266 Board Support**
   - File → Preferences → Additional Board Manager URLs
   - ESP32: `https://dl.espressif.com/dl/package_esp32_index.json`
   - ESP8266: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`

3. **Install Required Libraries**
   ```
   - PubSubClient (for MQTT)
   - DHT sensor library (by Adafruit)
   - ArduinoJson (version 6.x)
   ```
   Tools → Manage Libraries → Search and install each library

4. **Upload Firmware**
   - Open `irrigation_controller.ino`
   - Update WiFi credentials and MQTT settings in `config.h`
   - Select your board and port
   - Click Upload

### Python Cloud Service Setup

1. **Install Python 3.8+**
   ```bash
   python --version  # Check version
   ```

2. **Install Dependencies**
   ```bash
   pip install -r requirements.txt
   ```

3. **Configure Cloud Service**
   - Edit `cloud_service.py` with your MQTT broker details
   
4. **Run Cloud Service**
   ```bash
   python cloud_service.py
   ```

## Configuration

### WiFi Settings
Edit `config.h`:
```cpp
#define WIFI_SSID "YourWiFiNetwork"
#define WIFI_PASSWORD "YourWiFiPassword"
```

### MQTT Broker Settings
Edit `config.h`:
```cpp
#define MQTT_SERVER "mqtt.example.com"
#define MQTT_PORT 1883
#define MQTT_USER "your_username"
#define MQTT_PASSWORD "your_password"
```

### Soil Moisture Thresholds
Edit `config.h`:
```cpp
#define SOIL_DRY_THRESHOLD 30  // Start irrigation below 30%
#define SOIL_WET_THRESHOLD 70  // Stop irrigation above 70%
```

### Schedule Settings
Default schedule runs at 6:00 AM and 6:00 PM daily.
Modify in code or send MQTT command:
```json
{
  "schedule": {
    "hour": 6,
    "minute": 0,
    "duration": 300,
    "enabled": true
  }
}
```

## MQTT Topics

### Status Topic: `irrigation/status`
Published every 5 minutes with system status:
```json
{
  "pump_running": false,
  "auto_mode": true,
  "manual_override": false,
  "soil_moisture": 45.2,
  "temperature": 22.5,
  "humidity": 65.3,
  "schedule_hour": 6,
  "schedule_minute": 0,
  "schedule_duration": 300,
  "schedule_enabled": true,
  "uptime": 3600
}
```

### Sensor Topic: `irrigation/sensor`
Published every 1 minute with sensor readings:
```json
{
  "soil_moisture": 45.2,
  "temperature": 22.5,
  "humidity": 65.3,
  "timestamp": 1234567890
}
```

### Command Topic: `irrigation/command`
Subscribe to receive commands:

**Start Pump:**
```json
{"pump": "on"}
```

**Stop Pump:**
```json
{"pump": "off"}
```

**Switch to Auto Mode:**
```json
{"mode": "auto"}
```

**Switch to Manual Mode:**
```json
{"mode": "manual"}
```

**Update Schedule:**
```json
{
  "schedule": {
    "hour": 18,
    "minute": 30,
    "duration": 600,
    "enabled": true
  }
}
```

## Usage

### Automatic Mode (Default)
The system automatically monitors soil moisture and activates the pump when:
- Soil moisture drops below the dry threshold (30%)
- System stops when moisture exceeds wet threshold (70%)

### Manual Mode
Control the pump manually via MQTT commands while disabling automatic control.

### Scheduled Irrigation
Set up regular irrigation schedules that run at specific times regardless of soil moisture.

## Calibration

### Soil Moisture Sensor Calibration
1. Insert sensor in completely dry soil/air
2. Note the ADC reading (typically ~4095)
3. Insert sensor in water
4. Note the ADC reading (typically ~1500)
5. Update `air_value` and `water_value` in `config.h`

### Flow Rate Calibration
1. Run pump for exactly 60 seconds
2. Measure water output in milliliters
3. Calculate flow rate (ml/sec)
4. Update in `pump_controller.py`

## Monitoring and Maintenance

### Check System Status
- Monitor via MQTT status messages
- View logs in Serial Monitor (115200 baud)
- Check LED status indicator

### Regular Maintenance
- Clean soil moisture sensor monthly
- Check pump operation monthly
- Verify sensor readings are reasonable
- Update firmware when new versions available

### Troubleshooting

**WiFi Not Connecting:**
- Verify SSID and password
- Check signal strength
- Ensure 2.4GHz network (ESP8266 only supports 2.4GHz)

**Sensors Reading Invalid Data:**
- Check wiring connections
- Verify sensor power supply
- Replace faulty sensors

**Pump Not Starting:**
- Check relay connections
- Verify pump power supply
- Check safety limits (daily runtime, minimum off-time)
- Review logs for error messages

**MQTT Connection Failed:**
- Verify broker address and credentials
- Check firewall settings
- Test broker connectivity separately

## Architecture

```
┌─────────────────────────────────────────┐
│         ESP32/ESP8266                   │
│  ┌───────────────────────────────────┐  │
│  │   Main Controller Loop            │  │
│  │   - WiFi Connection               │  │
│  │   - MQTT Communication            │  │
│  │   - Sensor Reading                │  │
│  │   - Pump Control                  │  │
│  │   - Schedule Management           │  │
│  └───────────────────────────────────┘  │
│           ▲  │  │  │  ▼                 │
│           │  │  │  │  │                 │
│  ┌────────┘  │  │  │  └────────┐       │
│  │           │  │  │           │       │
│ Soil      DHT │  │ Relay      LED      │
│ Sensor    Sensor  │ Module            │
│                   │                     │
└───────────────────┼─────────────────────┘
                    │
                    ▼
              Water Pump
                    │
                    ▼
          Irrigation System

         ┌─────────────┐
         │   Internet  │
         └──────┬──────┘
                │
         ┌──────▼──────┐
         │ MQTT Broker │
         └──────┬──────┘
                │
    ┌───────────┴───────────┐
    │                       │
┌───▼────┐           ┌──────▼─────┐
│ Cloud  │           │   Mobile   │
│ Service│           │    App     │
└────────┘           └────────────┘
```

## Security Considerations

1. **WiFi Security**: Use WPA2 encryption
2. **MQTT Authentication**: Always use username/password
3. **TLS/SSL**: Enable for production deployments
4. **Firmware Updates**: Keep firmware up to date
5. **Access Control**: Limit MQTT topic access

## Future Enhancements

- [ ] Weather API integration to skip irrigation on rainy days
- [ ] Multiple sensor support (multiple soil moisture sensors)
- [ ] Water flow meter for accurate usage tracking
- [ ] Rain sensor integration
- [ ] Mobile app for easier control
- [ ] Machine learning for optimal irrigation scheduling
- [ ] Integration with smart home systems (Home Assistant, etc.)

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

This project is open source and available under the MIT License.

## Support

For questions and support, please open an issue on the GitHub repository.

## Credits

Developed for automated irrigation control using ESP32/ESP8266 microcontrollers.