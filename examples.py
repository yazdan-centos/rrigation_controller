"""
Example usage of the irrigation controller components
This demonstrates how to use the various modules
"""

import time
import logging
from datetime import datetime

# Import our modules
from sensor_manager import SensorManager
from pump_controller import PumpController
from scheduler import IrrigationScheduler, ScheduleRule

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


def example_sensor_reading():
    """Example: Reading sensors"""
    print("\n=== Sensor Reading Example ===")
    
    # Initialize sensor manager
    manager = SensorManager(soil_pin=34, dht_pin=4)
    
    # Take some readings
    for i in range(3):
        reading = manager.read_all()
        print(f"\nReading {i+1}:")
        print(f"  Soil Moisture: {reading.soil_moisture:.1f}%")
        print(f"  Temperature: {reading.temperature:.1f}°C")
        print(f"  Humidity: {reading.humidity:.1f}%")
        time.sleep(2)
    
    # Get average
    avg = manager.get_average_reading(minutes=1)
    if avg:
        print(f"\nAverage over last minute:")
        print(f"  Soil Moisture: {avg.soil_moisture:.1f}%")
        print(f"  Temperature: {avg.temperature:.1f}°C")
        print(f"  Humidity: {avg.humidity:.1f}%")


def example_pump_control():
    """Example: Controlling the pump"""
    print("\n=== Pump Control Example ===")
    
    # Initialize pump controller
    pump = PumpController(relay_pin=5, flow_rate_ml_per_sec=100.0)
    
    # Check if can start
    can_start, reason = pump.can_start()
    print(f"\nCan start pump? {can_start}")
    print(f"Reason: {reason}")
    
    if can_start:
        # Start pump
        print("\nStarting pump...")
        if pump.start("Test irrigation"):
            # Run for 10 seconds
            for i in range(10):
                runtime = pump.get_runtime()
                print(f"Pump running for {runtime}s...")
                time.sleep(1)
            
            # Stop pump
            print("\nStopping pump...")
            pump.stop("Test complete")
    
    # Get status
    status = pump.get_status()
    print(f"\nPump Status:")
    print(f"  Is Running: {status['is_running']}")
    print(f"  Today's Runtime: {status['today_runtime']}s")
    print(f"  Remaining Daily: {status['remaining_daily_runtime']}s")
    
    # Get statistics
    stats = pump.get_statistics(days=7)
    print(f"\nPump Statistics (Last 7 Days):")
    print(f"  Total Runs: {stats['total_runs']}")
    print(f"  Total Runtime: {stats['total_runtime_seconds']}s")
    print(f"  Total Water: {stats['total_water_ml']:.0f}ml")


def example_scheduling():
    """Example: Working with schedules"""
    print("\n=== Scheduling Example ===")
    
    # Create scheduler
    scheduler = IrrigationScheduler()
    
    # Add morning schedule (weekdays only)
    morning = ScheduleRule(
        rule_id="morning",
        hour=6,
        minute=0,
        duration=300,  # 5 minutes
        days_of_week=[0, 1, 2, 3, 4],  # Monday-Friday
        enabled=True
    )
    scheduler.add_rule(morning)
    print(f"\nAdded morning schedule: 6:00 AM (weekdays)")
    
    # Add evening schedule (all days)
    evening = ScheduleRule(
        rule_id="evening",
        hour=18,
        minute=0,
        duration=600,  # 10 minutes
        days_of_week=list(range(7)),  # All days
        enabled=True
    )
    scheduler.add_rule(evening)
    print(f"Added evening schedule: 6:00 PM (daily)")
    
    # Check what should run now
    current_time = datetime.now()
    print(f"\nCurrent time: {current_time.strftime('%Y-%m-%d %H:%M')}")
    
    due_rules = scheduler.get_due_rules(current_time)
    if due_rules:
        print("Rules that should run now:")
        for rule in due_rules:
            print(f"  - {rule.rule_id}")
    else:
        print("No rules due right now")
    
    # Get next run times
    print("\nNext scheduled runs:")
    for rule in scheduler.get_all_rules():
        next_run = scheduler.get_next_run(rule.rule_id)
        if next_run:
            print(f"  {rule.rule_id}: {next_run.strftime('%Y-%m-%d %H:%M')}")
    
    # Save schedules
    scheduler.save_to_file("example_schedules.json")
    print("\nSchedules saved to example_schedules.json")


def example_integrated_system():
    """Example: Integrated system operation"""
    print("\n=== Integrated System Example ===")
    
    # Initialize components
    sensors = SensorManager(soil_pin=34, dht_pin=4)
    pump = PumpController(relay_pin=5)
    
    # Thresholds
    DRY_THRESHOLD = 30
    WET_THRESHOLD = 70
    
    # Simulation loop
    print("\nRunning automatic irrigation control...")
    for cycle in range(5):
        print(f"\n--- Cycle {cycle + 1} ---")
        
        # Read sensors
        reading = sensors.read_all()
        print(f"Soil Moisture: {reading.soil_moisture:.1f}%")
        
        # Auto control logic
        if reading.soil_moisture < DRY_THRESHOLD and not pump.is_running:
            print("Soil is dry! Starting irrigation...")
            pump.start("Auto: Soil too dry")
            
        elif reading.soil_moisture > WET_THRESHOLD and pump.is_running:
            print("Soil is wet enough! Stopping irrigation...")
            pump.stop("Auto: Soil sufficiently wet")
            
        elif pump.is_running:
            runtime = pump.get_runtime()
            print(f"Irrigation in progress... (running for {runtime}s)")
        
        else:
            print("Soil moisture OK, no action needed")
        
        # Check safety
        is_safe, message = pump.check_safety()
        if not is_safe:
            print(f"SAFETY CHECK: {message}")
            pump.emergency_stop()
        
        time.sleep(2)
    
    # Stop pump if still running
    if pump.is_running:
        pump.stop("Simulation complete")
    
    # Final statistics
    stats = pump.get_statistics(days=1)
    print(f"\nFinal Statistics:")
    print(f"  Total runs: {stats['total_runs']}")
    print(f"  Total water used: {stats['total_water_ml']:.0f}ml")


def main():
    """Run all examples"""
    print("=" * 60)
    print("IRRIGATION CONTROLLER - EXAMPLE USAGE")
    print("=" * 60)
    
    try:
        # Run examples
        example_sensor_reading()
        time.sleep(1)
        
        example_pump_control()
        time.sleep(1)
        
        example_scheduling()
        time.sleep(1)
        
        example_integrated_system()
        
        print("\n" + "=" * 60)
        print("All examples completed successfully!")
        print("=" * 60)
        
    except KeyboardInterrupt:
        print("\n\nExamples interrupted by user")
    except Exception as e:
        logger.error(f"Error running examples: {e}", exc_info=True)


if __name__ == "__main__":
    main()
