# Circuit Diagram and Hardware Connections

## Basic Single-Zone Setup

### ESP32 Wiring Diagram

```
ESP32 Development Board
┌─────────────────────────┐
│                         │
│   3.3V ──────┬──────────┼─── 3.3V to DHT22 VCC
│              │          │
│              └──────────┼─── 3.3V to Relay VCC (if 3.3V relay)
│                         │
│   GND ───────┬──────────┼─── GND to DHT22 GND
│              │          │
│              ├──────────┼─── GND to Relay GND
│              │          │
│              └──────────┼─── GND to Soil Sensor GND
│                         │
│   GPIO34 ───────────────┼─── Signal from Soil Moisture Sensor
│                         │
│   GPIO4 ────────────────┼─── Data pin of DHT22
│                         │
│   GPIO5 ────────────────┼─── Control pin to Relay IN
│                         │
│   GPIO2 (Built-in LED)  │
│                         │
│   5V ───────────────────┼─── 5V to Soil Sensor VCC
│                         │
└─────────────────────────┘

Soil Moisture Sensor          DHT22 Temperature/Humidity
┌──────────────┐             ┌────────────┐
│   VCC   GND  │             │  VCC  DATA │
│    │     │   │             │   │    │   │
│    │     │   │             │   │    │   │
└────┼─────┼───┘             └───┼────┼───┘
     │     │                     │    │
     │     │                     │    │
  To 5V  To GND             To 3.3V  To GPIO4
     │                            │
  Signal                        GND
     │                            │
 To GPIO34                    To GND


5V Relay Module                    Water Pump/Solenoid
┌────────────────┐                ┌──────────────┐
│  VCC  GND  IN  │                │  12V DC      │
│   │    │   │   │                │  Water Pump  │
│   │    │   │   │                │              │
└───┼────┼───┼───┘                └──────┬───────┘
    │    │   │                           │
    │    │   │                           │
To 3.3V  │   │                      ┌────┴────┐
    To GND   │                      │ NO  COM │
         To GPIO5                   │   NC    │
                                    └─────────┘
                                    Relay Contacts

                        12V Power Supply
                        ┌──────┐
                        │  +   │
                        │  -   │
                        └──┬─┬─┘
                           │ │
                    ───────┘ └─── To Relay COM
                    │
                    └─────────── To Pump -
```

## Component Details

### 1. Soil Moisture Sensor
- **Type**: Capacitive (recommended) or Resistive
- **Operating Voltage**: 3.3V - 5V
- **Output**: Analog (0-3.3V or 0-5V)
- **Connection**:
  - VCC → 5V (ESP32)
  - GND → GND
  - AOUT → GPIO34 (ESP32) or A0 (ESP8266)

### 2. DHT22 Temperature & Humidity Sensor
- **Operating Voltage**: 3.3V - 5V
- **Output**: Digital (single-wire protocol)
- **Connection**:
  - VCC → 3.3V
  - GND → GND
  - DATA → GPIO4 (ESP32) or D4 (ESP8266)
- **Note**: Add 10kΩ pull-up resistor between DATA and VCC

### 3. Relay Module
- **Type**: 5V Single Channel Relay
- **Operating Voltage**: 3.3V - 5V
- **Control Signal**: Digital (HIGH/LOW)
- **Contact Rating**: 10A @ 250VAC / 10A @ 30VDC
- **Connection**:
  - VCC → 3.3V or 5V
  - GND → GND
  - IN → GPIO5 (ESP32) or D5 (ESP8266)

### 4. Water Pump
- **Type**: DC submersible pump or solenoid valve
- **Operating Voltage**: 12V DC (typical)
- **Current Draw**: 0.5A - 2A (depending on pump)
- **Connection**:
  - Connect pump through relay contacts
  - Use separate 12V power supply
  - Positive → Relay NO (Normally Open)
  - Negative → Power supply ground

## Multi-Zone Setup (4 Zones)

```
ESP32                  4-Channel Relay Module
┌─────────┐           ┌──────────────────────┐
│         │           │  IN1  IN2  IN3  IN4  │
│ GPIO5 ──┼───────────┼───┘    │    │    │   │
│ GPIO6 ──┼───────────┼────────┘    │    │   │
│ GPIO7 ──┼───────────┼─────────────┘    │   │
│ GPIO8 ──┼───────────┼──────────────────┘   │
│         │           │                       │
│  GND ───┼───────────┼─ GND                 │
│  VCC ───┼───────────┼─ VCC                 │
└─────────┘           └───┬───┬───┬───┬──────┘
                          │   │   │   │
                      Zone1  Zone2  Zone3  Zone4
                      Pump   Pump   Pump   Pump
```

## Power Supply Considerations

### Option 1: USB Power + Separate Pump Power
```
USB 5V (2A)              12V Power Supply (2A)
    │                           │
    │                           │
    └─── ESP32                  └─── Pump (via relay)
    │
    └─── Sensors
    │
    └─── Relay Module
```

### Option 2: Single Power Supply with Regulators
```
12V Power Supply (3A)
    │
    ├─── Buck Converter (12V → 5V)
    │         │
    │         └─── ESP32
    │         │
    │         └─── Sensors
    │
    └─── Pump (via relay)
```

## Safety Features

### 1. Flyback Diode
Add a flyback diode across pump terminals to protect relay from back EMF:
```
     Pump Motor
     ┌────────┐
     │        │
  ───┤+      -├───
     │   ┌──┐ │
     └───┤  ├─┘
         │  │
         └──┘
       Diode (1N4007)
       (Cathode to +)
```

### 2. Fuse Protection
Add fuse inline with pump power:
```
12V Supply → Fuse (2A) → Relay → Pump
```

## Enclosure and Weatherproofing

### Outdoor Installation Requirements
1. **IP65 Rated Enclosure**: Protects electronics from water
2. **Cable Glands**: Sealed entry points for wires
3. **Mounting**: Secure above ground level
4. **Ventilation**: Allow heat dissipation

### Sensor Placement
- **Soil Moisture**: Insert 2-4 inches into soil near plant roots
- **DHT22**: Mount in shaded area, protected from direct rain
- **Enclosure**: Mount at least 1 meter above ground

## Testing the Circuit

### Step 1: Power Test (Without Pump)
1. Connect ESP32, sensors, and relay module
2. DO NOT connect pump yet
3. Power on and verify:
   - ESP32 LED lights up
   - Relay LED indicator (if present)
   - No smoke or burning smell

### Step 2: Sensor Test
1. Upload test firmware
2. Open Serial Monitor (115200 baud)
3. Verify sensor readings:
   - Soil moisture: 0-100%
   - Temperature: reasonable value
   - Humidity: reasonable value

### Step 3: Relay Test
1. Send manual pump ON command
2. Verify relay clicks (audible)
3. Measure voltage across relay contacts (should be 0V when ON)
4. Send pump OFF command
5. Measure voltage across relay contacts (should be supply voltage when OFF)

### Step 4: Pump Test
1. Connect pump with separate power supply
2. Test manually with pump ON command
3. Verify pump runs for specified duration
4. Monitor current draw (should be within pump specifications)

## Bill of Materials (BOM)

### Essential Components
| Component | Quantity | Estimated Cost |
|-----------|----------|---------------|
| ESP32 Development Board | 1 | $8-15 |
| Soil Moisture Sensor (Capacitive) | 1 | $3-5 |
| DHT22 Temperature/Humidity Sensor | 1 | $5-10 |
| 5V Relay Module (1-channel) | 1 | $2-5 |
| 12V DC Water Pump | 1 | $5-15 |
| 12V Power Supply (2A) | 1 | $8-12 |
| USB Cable for ESP32 | 1 | $3-5 |
| Jumper Wires | Set | $3-5 |
| Enclosure (IP65) | 1 | $10-20 |
| **Total** | | **~$50-100** |

### Optional Components
| Component | Quantity | Estimated Cost |
|-----------|----------|---------------|
| 4-Channel Relay Module (for multi-zone) | 1 | $5-10 |
| Additional Soil Sensors | 1-3 | $9-15 |
| Additional Pumps | 1-3 | $15-45 |
| LCD Display (I2C) | 1 | $5-10 |
| Push Buttons | 2-3 | $2-5 |
| Rain Sensor | 1 | $3-5 |
| Water Flow Meter | 1 | $8-15 |

## Common Issues and Solutions

### Issue: Relay not switching
- **Check**: Relay module VCC voltage (should be 3.3V or 5V)
- **Check**: GPIO pin is set as OUTPUT
- **Check**: Relay control pin connection
- **Solution**: Some relays need 5V signal; use level shifter if needed

### Issue: Soil moisture reading constant
- **Check**: Sensor power connection
- **Check**: Analog pin connection
- **Solution**: Try different analog pin or replace sensor

### Issue: DHT sensor timeout
- **Check**: 10kΩ pull-up resistor on data line
- **Check**: Sensor power supply (3.3V or 5V)
- **Solution**: Add delay between readings (minimum 2 seconds)

### Issue: Pump runs continuously
- **Check**: Relay is normally open (NO), not normally closed (NC)
- **Check**: Code logic for pump control
- **Solution**: Verify relay wiring and safety timeout code

## Advanced: Solar Power Setup

```
Solar Panel (20W)
       │
       └─── Solar Charge Controller
                  │
                  └─── 12V Battery (7Ah)
                         │
                         ├─── Buck Converter → ESP32
                         │
                         └─── Pump (via relay)
```

### Components for Solar Setup
- 20W Solar Panel
- 12V/7Ah Lead-Acid or LiPo Battery
- Solar Charge Controller (10A)
- Buck Converter (12V → 5V, 3A)
- Total Cost: Additional $50-80

This enables off-grid operation for remote installations.
