"""
Irrigation Controller - Python Cloud Service
This service provides cloud connectivity and monitoring for the irrigation system
"""

import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime
import sqlite3
import logging

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class IrrigationCloudService:
    """Cloud service for irrigation controller"""
    
    def __init__(self, mqtt_broker, mqtt_port, mqtt_user, mqtt_password):
        self.mqtt_broker = mqtt_broker
        self.mqtt_port = mqtt_port
        self.mqtt_user = mqtt_user
        self.mqtt_password = mqtt_password
        
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.username_pw_set(mqtt_user, mqtt_password)
        
        # Initialize database
        self.init_database()
        
    def init_database(self):
        """Initialize SQLite database for storing sensor data"""
        self.conn = sqlite3.connect('irrigation_data.db')
        cursor = self.conn.cursor()
        
        # Sensor data table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS sensor_readings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                soil_moisture REAL,
                temperature REAL,
                humidity REAL
            )
        ''')
        
        # System status table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS system_status (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                pump_running BOOLEAN,
                auto_mode BOOLEAN,
                uptime INTEGER
            )
        ''')
        
        # Irrigation events table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS irrigation_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                start_time DATETIME,
                end_time DATETIME,
                duration_seconds INTEGER,
                reason TEXT,
                water_volume_estimate REAL
            )
        ''')
        
        self.conn.commit()
        logger.info("Database initialized")
        
    def on_connect(self, client, userdata, flags, rc):
        """Callback when connected to MQTT broker"""
        if rc == 0:
            logger.info("Connected to MQTT broker")
            client.subscribe("irrigation/#")
        else:
            logger.error(f"Connection failed with code {rc}")
            
    def on_message(self, client, userdata, msg):
        """Callback when message received"""
        try:
            topic = msg.topic
            payload = json.loads(msg.payload.decode())
            
            logger.info(f"Received message on {topic}")
            
            if topic == "irrigation/sensor":
                self.store_sensor_data(payload)
            elif topic == "irrigation/status":
                self.store_system_status(payload)
            elif topic == "irrigation/alert":
                self.handle_alert(payload)
                
        except Exception as e:
            logger.error(f"Error processing message: {e}")
            
    def store_sensor_data(self, data):
        """Store sensor readings in database"""
        cursor = self.conn.cursor()
        cursor.execute('''
            INSERT INTO sensor_readings (soil_moisture, temperature, humidity)
            VALUES (?, ?, ?)
        ''', (
            data.get('soil_moisture'),
            data.get('temperature'),
            data.get('humidity')
        ))
        self.conn.commit()
        logger.info(f"Stored sensor data: {data}")
        
    def store_system_status(self, data):
        """Store system status in database"""
        cursor = self.conn.cursor()
        cursor.execute('''
            INSERT INTO system_status (pump_running, auto_mode, uptime)
            VALUES (?, ?, ?)
        ''', (
            data.get('pump_running'),
            data.get('auto_mode'),
            data.get('uptime')
        ))
        self.conn.commit()
        logger.info(f"Stored system status")
        
    def handle_alert(self, data):
        """Handle system alerts"""
        logger.warning(f"ALERT: {data}")
        # In production, send email/SMS notifications here
        
    def send_command(self, command):
        """Send command to irrigation controller"""
        topic = "irrigation/command"
        payload = json.dumps(command)
        self.client.publish(topic, payload)
        logger.info(f"Sent command: {command}")
        
    def get_sensor_history(self, hours=24):
        """Get sensor readings from last N hours"""
        cursor = self.conn.cursor()
        cursor.execute('''
            SELECT timestamp, soil_moisture, temperature, humidity
            FROM sensor_readings
            WHERE timestamp > datetime('now', '-{} hours')
            ORDER BY timestamp DESC
        '''.format(hours))
        return cursor.fetchall()
        
    def get_irrigation_stats(self, days=7):
        """Get irrigation statistics"""
        cursor = self.conn.cursor()
        cursor.execute('''
            SELECT 
                COUNT(*) as irrigation_count,
                SUM(duration_seconds) as total_duration,
                AVG(duration_seconds) as avg_duration
            FROM irrigation_events
            WHERE start_time > datetime('now', '-{} days')
        '''.format(days))
        return cursor.fetchone()
        
    def start(self):
        """Start the cloud service"""
        logger.info("Starting irrigation cloud service...")
        self.client.connect(self.mqtt_broker, self.mqtt_port, 60)
        self.client.loop_forever()
        
    def stop(self):
        """Stop the cloud service"""
        logger.info("Stopping irrigation cloud service...")
        self.client.disconnect()
        self.conn.close()


def main():
    """Main entry point"""
    # Configuration
    MQTT_BROKER = "mqtt.example.com"
    MQTT_PORT = 1883
    MQTT_USER = "your_mqtt_user"
    MQTT_PASSWORD = "your_mqtt_password"
    
    # Create and start service
    service = IrrigationCloudService(
        MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD
    )
    
    try:
        service.start()
    except KeyboardInterrupt:
        logger.info("Shutting down...")
        service.stop()


if __name__ == "__main__":
    main()
