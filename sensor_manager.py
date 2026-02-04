"""
Sensor Manager - Handles all sensor readings and calibration
"""

import time
from typing import Dict, Optional, List
from dataclasses import dataclass
from datetime import datetime
import logging

logger = logging.getLogger(__name__)


@dataclass
class SensorReading:
    """Container for sensor readings"""
    timestamp: datetime
    soil_moisture: float
    temperature: float
    humidity: float
    raw_soil_value: Optional[int] = None
    
    def is_valid(self) -> bool:
        """Check if all readings are valid"""
        return (
            0 <= self.soil_moisture <= 100 and
            -50 <= self.temperature <= 80 and
            0 <= self.humidity <= 100
        )
        
    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            'timestamp': self.timestamp.isoformat(),
            'soil_moisture': round(self.soil_moisture, 2),
            'temperature': round(self.temperature, 2),
            'humidity': round(self.humidity, 2),
            'raw_soil_value': self.raw_soil_value
        }


class SoilMoistureSensor:
    """Manages soil moisture sensor with calibration"""
    
    def __init__(self, pin: int, air_value: int = 4095, water_value: int = 1500):
        """
        Initialize soil moisture sensor
        
        Args:
            pin: Analog pin number
            air_value: ADC value when sensor is in air (dry)
            water_value: ADC value when sensor is in water (wet)
        """
        self.pin = pin
        self.air_value = air_value
        self.water_value = water_value
        self.readings_buffer = []
        self.buffer_size = 10
        
    def read_raw(self) -> int:
        """Read raw ADC value (simulated for Python)"""
        # In actual implementation, this would read from GPIO
        import random
        return random.randint(1500, 4095)
        
    def read_moisture(self) -> float:
        """Read and convert to moisture percentage"""
        raw = self.read_raw()
        
        # Convert to percentage (inverse: high ADC = dry, low ADC = wet)
        moisture = ((self.air_value - raw) / (self.air_value - self.water_value)) * 100
        moisture = max(0, min(100, moisture))
        
        # Add to buffer for averaging
        self.readings_buffer.append(moisture)
        if len(self.readings_buffer) > self.buffer_size:
            self.readings_buffer.pop(0)
            
        return moisture
        
    def get_averaged_moisture(self) -> float:
        """Get averaged moisture reading"""
        if not self.readings_buffer:
            return self.read_moisture()
        return sum(self.readings_buffer) / len(self.readings_buffer)
        
    def calibrate(self, in_air: bool):
        """Calibrate sensor
        
        Args:
            in_air: True if sensor is in air (dry), False if in water (wet)
        """
        readings = []
        for _ in range(20):
            readings.append(self.read_raw())
            time.sleep(0.1)
            
        avg_value = sum(readings) / len(readings)
        
        if in_air:
            self.air_value = int(avg_value)
            logger.info(f"Calibrated air value: {self.air_value}")
        else:
            self.water_value = int(avg_value)
            logger.info(f"Calibrated water value: {self.water_value}")


class TemperatureHumiditySensor:
    """Manages DHT temperature and humidity sensor"""
    
    def __init__(self, pin: int, sensor_type: str = "DHT22"):
        """
        Initialize DHT sensor
        
        Args:
            pin: Digital pin number
            sensor_type: "DHT11" or "DHT22"
        """
        self.pin = pin
        self.sensor_type = sensor_type
        self.last_reading_time = None
        self.min_read_interval = 2.0  # seconds
        self.last_temp = None
        self.last_humidity = None
        
    def read(self) -> tuple:
        """Read temperature and humidity"""
        # Check if enough time has passed since last reading
        current_time = time.time()
        if self.last_reading_time:
            elapsed = current_time - self.last_reading_time
            if elapsed < self.min_read_interval:
                return self.last_temp, self.last_humidity
                
        # Simulate reading (in real implementation, use DHT library)
        import random
        temperature = random.uniform(15, 35)
        humidity = random.uniform(30, 80)
        
        self.last_temp = temperature
        self.last_humidity = humidity
        self.last_reading_time = current_time
        
        return temperature, humidity


class SensorManager:
    """Manages all sensors and provides unified readings"""
    
    def __init__(self, soil_pin: int, dht_pin: int):
        """Initialize sensor manager"""
        self.soil_sensor = SoilMoistureSensor(soil_pin)
        self.dht_sensor = TemperatureHumiditySensor(dht_pin)
        self.reading_history: List[SensorReading] = []
        self.max_history = 1000
        
    def read_all(self) -> SensorReading:
        """Read all sensors and return combined reading"""
        # Read soil moisture
        soil_moisture = self.soil_sensor.get_averaged_moisture()
        raw_soil = self.soil_sensor.read_raw()
        
        # Read temperature and humidity
        temperature, humidity = self.dht_sensor.read()
        
        # Create reading object
        reading = SensorReading(
            timestamp=datetime.now(),
            soil_moisture=soil_moisture,
            temperature=temperature,
            humidity=humidity,
            raw_soil_value=raw_soil
        )
        
        # Store in history
        self.reading_history.append(reading)
        if len(self.reading_history) > self.max_history:
            self.reading_history.pop(0)
            
        logger.info(f"Sensor reading: moisture={soil_moisture:.1f}%, "
                   f"temp={temperature:.1f}°C, humidity={humidity:.1f}%")
        
        return reading
        
    def get_average_reading(self, minutes: int = 5) -> Optional[SensorReading]:
        """Get average reading over last N minutes"""
        if not self.reading_history:
            return None
            
        cutoff_time = datetime.now().timestamp() - (minutes * 60)
        recent_readings = [
            r for r in self.reading_history 
            if r.timestamp.timestamp() > cutoff_time
        ]
        
        if not recent_readings:
            return None
            
        avg_reading = SensorReading(
            timestamp=datetime.now(),
            soil_moisture=sum(r.soil_moisture for r in recent_readings) / len(recent_readings),
            temperature=sum(r.temperature for r in recent_readings) / len(recent_readings),
            humidity=sum(r.humidity for r in recent_readings) / len(recent_readings)
        )
        
        return avg_reading
        
    def get_trends(self, minutes: int = 30) -> Dict[str, str]:
        """Analyze sensor trends over time"""
        if len(self.reading_history) < 2:
            return {
                'soil_moisture': 'stable',
                'temperature': 'stable',
                'humidity': 'stable'
            }
            
        cutoff_time = datetime.now().timestamp() - (minutes * 60)
        recent_readings = [
            r for r in self.reading_history 
            if r.timestamp.timestamp() > cutoff_time
        ]
        
        if len(recent_readings) < 2:
            return {
                'soil_moisture': 'stable',
                'temperature': 'stable',
                'humidity': 'stable'
            }
            
        # Calculate trends
        first = recent_readings[0]
        last = recent_readings[-1]
        
        def trend(old_val, new_val, threshold=2.0):
            diff = new_val - old_val
            if abs(diff) < threshold:
                return 'stable'
            return 'increasing' if diff > 0 else 'decreasing'
            
        return {
            'soil_moisture': trend(first.soil_moisture, last.soil_moisture, 5.0),
            'temperature': trend(first.temperature, last.temperature, 2.0),
            'humidity': trend(first.humidity, last.humidity, 5.0)
        }
        
    def check_sensor_health(self) -> Dict[str, bool]:
        """Check if all sensors are working properly"""
        health = {
            'soil_moisture': False,
            'temperature': False,
            'humidity': False,
            'overall': False
        }
        
        try:
            reading = self.read_all()
            
            # Check soil moisture
            health['soil_moisture'] = 0 <= reading.soil_moisture <= 100
            
            # Check temperature
            health['temperature'] = -50 <= reading.temperature <= 80
            
            # Check humidity
            health['humidity'] = 0 <= reading.humidity <= 100
            
            health['overall'] = all([
                health['soil_moisture'],
                health['temperature'],
                health['humidity']
            ])
            
        except Exception as e:
            logger.error(f"Sensor health check failed: {e}")
            
        return health


if __name__ == "__main__":
    # Example usage
    logging.basicConfig(level=logging.INFO)
    
    manager = SensorManager(soil_pin=34, dht_pin=4)
    
    # Take some readings
    for i in range(5):
        reading = manager.read_all()
        print(f"Reading {i+1}: {reading.to_dict()}")
        time.sleep(2)
        
    # Get average
    avg = manager.get_average_reading(minutes=1)
    if avg:
        print(f"Average: {avg.to_dict()}")
        
    # Check trends
    trends = manager.get_trends(minutes=1)
    print(f"Trends: {trends}")
    
    # Health check
    health = manager.check_sensor_health()
    print(f"Sensor health: {health}")
