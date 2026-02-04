# API Documentation

## MQTT API

The irrigation controller communicates via MQTT protocol. All messages use JSON format.

### Connection Parameters

```python
MQTT_BROKER = "mqtt.example.com"
MQTT_PORT = 1883
MQTT_USER = "username"
MQTT_PASSWORD = "password"
```

### Topics

#### 1. Status Topic: `irrigation/status`

**Published by**: Controller  
**Frequency**: Every 5 minutes  
**QoS**: 1

**Payload Structure**:
```json
{
  "device_id": "irrigation-001",
  "timestamp": 1612345678,
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
  "uptime": 3600,
  "wifi_rssi": -65,
  "firmware_version": "1.0.0"
}
```

**Field Descriptions**:
- `device_id`: Unique identifier for the device
- `timestamp`: Unix timestamp
- `pump_running`: Boolean indicating pump state
- `auto_mode`: Boolean indicating auto control mode
- `manual_override`: Boolean indicating manual override active
- `soil_moisture`: Soil moisture percentage (0-100)
- `temperature`: Temperature in Celsius
- `humidity`: Relative humidity percentage (0-100)
- `schedule_hour`: Scheduled irrigation hour (0-23)
- `schedule_minute`: Scheduled irrigation minute (0-59)
- `schedule_duration`: Scheduled duration in seconds
- `schedule_enabled`: Boolean indicating schedule is active
- `uptime`: Device uptime in seconds
- `wifi_rssi`: WiFi signal strength in dBm
- `firmware_version`: Firmware version string

#### 2. Sensor Topic: `irrigation/sensor`

**Published by**: Controller  
**Frequency**: Every 60 seconds  
**QoS**: 0

**Payload Structure**:
```json
{
  "device_id": "irrigation-001",
  "timestamp": 1612345678,
  "soil_moisture": 45.2,
  "temperature": 22.5,
  "humidity": 65.3,
  "raw_soil_value": 2450
}
```

#### 3. Command Topic: `irrigation/command`

**Published by**: Client/Cloud Service  
**Subscribed by**: Controller  
**QoS**: 1

##### Command: Start Pump
```json
{
  "pump": "on"
}
```

##### Command: Stop Pump
```json
{
  "pump": "off"
}
```

##### Command: Set Mode
```json
{
  "mode": "auto"
}
```
or
```json
{
  "mode": "manual"
}
```

##### Command: Update Schedule
```json
{
  "schedule": {
    "hour": 6,
    "minute": 30,
    "duration": 600,
    "enabled": true
  }
}
```

##### Command: Request Status
```json
{
  "command": "status"
}
```

#### 4. Alert Topic: `irrigation/alert`

**Published by**: Controller  
**Frequency**: On event  
**QoS**: 1

**Payload Structure**:
```json
{
  "device_id": "irrigation-001",
  "timestamp": 1612345678,
  "level": "warning",
  "type": "max_runtime_exceeded",
  "message": "Pump stopped: maximum runtime exceeded"
}
```

**Alert Levels**:
- `info`: Informational message
- `warning`: Warning condition
- `error`: Error condition
- `critical`: Critical condition requiring immediate attention

**Alert Types**:
- `max_runtime_exceeded`: Pump exceeded maximum runtime
- `sensor_failure`: Sensor not responding or invalid readings
- `wifi_disconnected`: WiFi connection lost
- `mqtt_disconnected`: MQTT connection lost
- `low_soil_moisture`: Soil moisture critically low
- `pump_start`: Pump started
- `pump_stop`: Pump stopped

## Python API

### Cloud Service

```python
from cloud_service import IrrigationCloudService

# Initialize service
service = IrrigationCloudService(
    mqtt_broker="mqtt.example.com",
    mqtt_port=1883,
    mqtt_user="username",
    mqtt_password="password"
)

# Send command to start pump
service.send_command({"pump": "on"})

# Send command to update schedule
service.send_command({
    "schedule": {
        "hour": 18,
        "minute": 0,
        "duration": 300,
        "enabled": True
    }
})

# Get sensor history (last 24 hours)
history = service.get_sensor_history(hours=24)

# Get irrigation statistics (last 7 days)
stats = service.get_irrigation_stats(days=7)
print(f"Irrigation count: {stats[0]}")
print(f"Total duration: {stats[1]}s")
print(f"Average duration: {stats[2]}s")

# Start service
service.start()
```

### Scheduler

```python
from scheduler import IrrigationScheduler, ScheduleRule
from datetime import datetime

# Create scheduler
scheduler = IrrigationScheduler()

# Add morning schedule (6 AM, weekdays only)
morning_rule = ScheduleRule(
    rule_id="morning",
    hour=6,
    minute=0,
    duration=300,  # 5 minutes
    days_of_week=[0, 1, 2, 3, 4],  # Monday-Friday
    enabled=True
)
scheduler.add_rule(morning_rule)

# Add evening schedule (6 PM, all days)
evening_rule = ScheduleRule(
    rule_id="evening",
    hour=18,
    minute=0,
    duration=600,  # 10 minutes
    days_of_week=list(range(7)),  # All days
    enabled=True
)
scheduler.add_rule(evening_rule)

# Check what should run now
due_rules = scheduler.get_due_rules(datetime.now())
for rule in due_rules:
    print(f"Should run: {rule.rule_id}")
    scheduler.mark_rule_executed(rule.rule_id)

# Get next run time
next_run = scheduler.get_next_run("morning")
print(f"Next morning irrigation: {next_run}")

# Save schedules to file
scheduler.save_to_file("schedules.json")

# Load schedules from file
scheduler.load_from_file("schedules.json")
```

### Sensor Manager

```python
from sensor_manager import SensorManager

# Initialize sensor manager
manager = SensorManager(soil_pin=34, dht_pin=4)

# Read all sensors
reading = manager.read_all()
print(f"Soil: {reading.soil_moisture}%")
print(f"Temp: {reading.temperature}°C")
print(f"Humidity: {reading.humidity}%")

# Get average reading over last 5 minutes
avg = manager.get_average_reading(minutes=5)
if avg:
    print(f"Average soil moisture: {avg.soil_moisture}%")

# Analyze trends
trends = manager.get_trends(minutes=30)
print(f"Soil moisture trend: {trends['soil_moisture']}")

# Check sensor health
health = manager.check_sensor_health()
if health['overall']:
    print("All sensors healthy")
else:
    print("Sensor issues detected")
```

### Pump Controller

```python
from pump_controller import PumpController, MultiZonePumpController

# Single zone controller
pump = PumpController(relay_pin=5, flow_rate_ml_per_sec=100.0)

# Check if can start
can_start, reason = pump.can_start()
if can_start:
    # Start pump
    if pump.start("Scheduled irrigation"):
        print("Pump started")
        
        # Run for some time
        time.sleep(60)
        
        # Stop pump
        pump.stop("Schedule complete")

# Get current status
status = pump.get_status()
print(f"Running: {status['is_running']}")
print(f"Today's runtime: {status['today_runtime']}s")

# Get statistics
stats = pump.get_statistics(days=7)
print(f"Total runs: {stats['total_runs']}")
print(f"Total water used: {stats['total_water_ml']}ml")

# Multi-zone controller
multi = MultiZonePumpController({
    'front_yard': 5,
    'back_yard': 6,
    'garden': 7
})

# Start specific zone
multi.start_zone('front_yard', "Scheduled irrigation")

# Stop all zones
multi.stop_all_zones()

# Get all status
all_status = multi.get_all_status()
for zone, status in all_status.items():
    print(f"{zone}: {status}")
```

## REST API (Optional)

If implementing a REST API wrapper around the MQTT system:

### Base URL
```
http://your-server:8000/api/v1
```

### Endpoints

#### GET /status
Get current system status.

**Response**:
```json
{
  "status": "ok",
  "data": {
    "pump_running": false,
    "auto_mode": true,
    "soil_moisture": 45.2,
    "temperature": 22.5,
    "humidity": 65.3
  }
}
```

#### POST /pump/start
Start the pump.

**Request**:
```json
{
  "reason": "Manual start via API"
}
```

**Response**:
```json
{
  "status": "ok",
  "message": "Pump started"
}
```

#### POST /pump/stop
Stop the pump.

**Response**:
```json
{
  "status": "ok",
  "message": "Pump stopped"
}
```

#### GET /sensors/history
Get sensor reading history.

**Query Parameters**:
- `hours`: Number of hours (default: 24)

**Response**:
```json
{
  "status": "ok",
  "data": [
    {
      "timestamp": "2024-02-04T12:00:00Z",
      "soil_moisture": 45.2,
      "temperature": 22.5,
      "humidity": 65.3
    }
  ]
}
```

#### POST /schedule
Update irrigation schedule.

**Request**:
```json
{
  "hour": 6,
  "minute": 0,
  "duration": 300,
  "enabled": true
}
```

**Response**:
```json
{
  "status": "ok",
  "message": "Schedule updated"
}
```

## Error Codes

### MQTT Error Responses

Commands may return error responses on the status topic:

```json
{
  "error": true,
  "error_code": "PUMP_ALREADY_RUNNING",
  "message": "Pump is already running"
}
```

**Error Codes**:
- `PUMP_ALREADY_RUNNING`: Pump is already in running state
- `PUMP_NOT_RUNNING`: Pump is not running
- `MAX_RUNTIME_EXCEEDED`: Maximum runtime limit reached
- `DAILY_LIMIT_REACHED`: Daily runtime limit reached
- `MIN_OFF_TIME`: Minimum off-time not elapsed
- `SENSOR_FAILURE`: Sensor malfunction detected
- `INVALID_COMMAND`: Unknown or malformed command
- `WIFI_ERROR`: WiFi connection error
- `MQTT_ERROR`: MQTT connection error

## Example Integrations

### Node-RED Flow

```json
[
  {
    "id": "mqtt-in",
    "type": "mqtt in",
    "topic": "irrigation/status",
    "broker": "mqtt-broker"
  },
  {
    "id": "check-moisture",
    "type": "function",
    "func": "if (msg.payload.soil_moisture < 30) {\n  msg.payload = {pump: 'on'};\n  return msg;\n}\nreturn null;"
  },
  {
    "id": "mqtt-out",
    "type": "mqtt out",
    "topic": "irrigation/command",
    "broker": "mqtt-broker"
  }
]
```

### Home Assistant Configuration

```yaml
mqtt:
  sensor:
    - name: "Garden Soil Moisture"
      state_topic: "irrigation/sensor"
      value_template: "{{ value_json.soil_moisture }}"
      unit_of_measurement: "%"
      
    - name: "Garden Temperature"
      state_topic: "irrigation/sensor"
      value_template: "{{ value_json.temperature }}"
      unit_of_measurement: "°C"
      
  switch:
    - name: "Garden Pump"
      command_topic: "irrigation/command"
      state_topic: "irrigation/status"
      payload_on: '{"pump":"on"}'
      payload_off: '{"pump":"off"}'
      state_on: "true"
      state_off: "false"
      value_template: "{{ value_json.pump_running }}"
```

### Python CLI Tool

```python
#!/usr/bin/env python3
import paho.mqtt.client as mqtt
import json
import sys

def send_command(command):
    client = mqtt.Client()
    client.username_pw_set("username", "password")
    client.connect("mqtt.example.com", 1883, 60)
    client.publish("irrigation/command", json.dumps(command))
    client.disconnect()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: irrigation_cli.py <command>")
        print("Commands: start, stop, auto, manual")
        sys.exit(1)
        
    cmd = sys.argv[1]
    if cmd == "start":
        send_command({"pump": "on"})
    elif cmd == "stop":
        send_command({"pump": "off"})
    elif cmd == "auto":
        send_command({"mode": "auto"})
    elif cmd == "manual":
        send_command({"mode": "manual"})
```

Usage:
```bash
./irrigation_cli.py start
./irrigation_cli.py stop
./irrigation_cli.py auto
```
