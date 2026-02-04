"""
Pump Controller - Manages water pump operations and safety
"""

import time
from datetime import datetime, timedelta
from typing import Optional, Dict, List
from dataclasses import dataclass
import logging

logger = logging.getLogger(__name__)


@dataclass
class PumpEvent:
    """Record of a pump operation"""
    start_time: datetime
    end_time: Optional[datetime]
    reason: str
    duration_seconds: Optional[int] = None
    water_volume_ml: Optional[float] = None
    
    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            'start_time': self.start_time.isoformat(),
            'end_time': self.end_time.isoformat() if self.end_time else None,
            'reason': self.reason,
            'duration_seconds': self.duration_seconds,
            'water_volume_ml': self.water_volume_ml
        }


class PumpController:
    """Controls water pump with safety features"""
    
    def __init__(self, relay_pin: int, flow_rate_ml_per_sec: float = 100.0):
        """
        Initialize pump controller
        
        Args:
            relay_pin: GPIO pin for relay control
            flow_rate_ml_per_sec: Estimated flow rate in ml per second
        """
        self.relay_pin = relay_pin
        self.flow_rate = flow_rate_ml_per_sec
        self.is_running = False
        self.current_event: Optional[PumpEvent] = None
        self.event_history: List[PumpEvent] = []
        
        # Safety limits
        self.max_runtime_seconds = 600  # 10 minutes
        self.min_off_time_seconds = 1800  # 30 minutes between runs
        self.max_daily_runtime = 3600  # 1 hour per day
        self.daily_runtime_reset_hour = 0  # Reset at midnight
        
        # State tracking
        self.today_runtime = 0
        self.last_runtime_reset = datetime.now().date()
        self.last_stop_time: Optional[datetime] = None
        
    def can_start(self) -> tuple[bool, str]:
        """
        Check if pump can be started safely
        
        Returns:
            (can_start, reason) tuple
        """
        # Check if already running
        if self.is_running:
            return False, "Pump is already running"
            
        # Check minimum off time
        if self.last_stop_time:
            time_since_stop = (datetime.now() - self.last_stop_time).total_seconds()
            if time_since_stop < self.min_off_time_seconds:
                remaining = self.min_off_time_seconds - time_since_stop
                return False, f"Must wait {int(remaining)}s before next run"
                
        # Reset daily runtime if it's a new day
        current_date = datetime.now().date()
        if current_date != self.last_runtime_reset:
            self.today_runtime = 0
            self.last_runtime_reset = current_date
            logger.info("Daily runtime counter reset")
            
        # Check daily limit
        if self.today_runtime >= self.max_daily_runtime:
            return False, "Daily runtime limit reached"
            
        return True, "OK"
        
    def start(self, reason: str = "Manual") -> bool:
        """
        Start the pump
        
        Args:
            reason: Reason for starting the pump
            
        Returns:
            True if started successfully
        """
        can_start, message = self.can_start()
        
        if not can_start:
            logger.warning(f"Cannot start pump: {message}")
            return False
            
        # Start the pump
        self._set_relay(True)
        self.is_running = True
        
        # Create event record
        self.current_event = PumpEvent(
            start_time=datetime.now(),
            end_time=None,
            reason=reason
        )
        
        logger.info(f"Pump started: {reason}")
        return True
        
    def stop(self, reason: str = "Manual") -> bool:
        """
        Stop the pump
        
        Args:
            reason: Reason for stopping the pump
            
        Returns:
            True if stopped successfully
        """
        if not self.is_running:
            logger.warning("Pump is not running")
            return False
            
        # Stop the pump
        self._set_relay(False)
        self.is_running = False
        self.last_stop_time = datetime.now()
        
        # Complete event record
        if self.current_event:
            self.current_event.end_time = self.last_stop_time
            duration = (self.current_event.end_time - self.current_event.start_time).total_seconds()
            self.current_event.duration_seconds = int(duration)
            self.current_event.water_volume_ml = duration * self.flow_rate
            
            # Update daily runtime
            self.today_runtime += duration
            
            # Store in history
            self.event_history.append(self.current_event)
            
            logger.info(f"Pump stopped: {reason}, ran for {duration:.0f}s")
            
        self.current_event = None
        return True
        
    def _set_relay(self, state: bool):
        """Set relay state (simulated for Python)"""
        # In real implementation, this would control GPIO
        logger.debug(f"Relay set to {'ON' if state else 'OFF'}")
        
    def get_runtime(self) -> Optional[int]:
        """Get current runtime in seconds"""
        if not self.is_running or not self.current_event:
            return None
            
        runtime = (datetime.now() - self.current_event.start_time).total_seconds()
        return int(runtime)
        
    def check_safety(self) -> tuple[bool, str]:
        """
        Check safety conditions during operation
        
        Returns:
            (is_safe, message) tuple
        """
        if not self.is_running:
            return True, "Pump not running"
            
        runtime = self.get_runtime()
        
        # Check maximum runtime
        if runtime and runtime > self.max_runtime_seconds:
            return False, "Maximum runtime exceeded"
            
        # Check daily limit (with current run)
        if self.today_runtime + runtime > self.max_daily_runtime:
            return False, "Daily runtime limit will be exceeded"
            
        return True, "OK"
        
    def emergency_stop(self):
        """Emergency stop - bypasses all checks"""
        self._set_relay(False)
        self.is_running = False
        logger.warning("EMERGENCY STOP activated")
        
    def get_statistics(self, days: int = 7) -> Dict:
        """Get pump usage statistics"""
        cutoff_date = datetime.now() - timedelta(days=days)
        
        recent_events = [
            e for e in self.event_history 
            if e.start_time > cutoff_date and e.duration_seconds
        ]
        
        if not recent_events:
            return {
                'total_runs': 0,
                'total_runtime_seconds': 0,
                'total_water_ml': 0,
                'average_runtime_seconds': 0
            }
            
        total_runtime = sum(e.duration_seconds for e in recent_events)
        total_water = sum(e.water_volume_ml or 0 for e in recent_events)
        
        return {
            'total_runs': len(recent_events),
            'total_runtime_seconds': total_runtime,
            'total_water_ml': total_water,
            'average_runtime_seconds': total_runtime / len(recent_events)
        }
        
    def get_status(self) -> Dict:
        """Get current pump status"""
        status = {
            'is_running': self.is_running,
            'current_runtime': self.get_runtime(),
            'today_runtime': int(self.today_runtime),
            'remaining_daily_runtime': int(self.max_daily_runtime - self.today_runtime),
            'can_start': self.can_start()[0],
            'last_event': None
        }
        
        if self.event_history:
            status['last_event'] = self.event_history[-1].to_dict()
            
        return status


class MultiZonePumpController:
    """Controller for multiple irrigation zones"""
    
    def __init__(self, zone_pins: Dict[str, int]):
        """
        Initialize multi-zone controller
        
        Args:
            zone_pins: Dictionary mapping zone names to relay pins
        """
        self.zones = {
            name: PumpController(pin)
            for name, pin in zone_pins.items()
        }
        
    def start_zone(self, zone_name: str, reason: str = "Manual") -> bool:
        """Start irrigation for a specific zone"""
        if zone_name not in self.zones:
            logger.error(f"Unknown zone: {zone_name}")
            return False
            
        return self.zones[zone_name].start(reason)
        
    def stop_zone(self, zone_name: str) -> bool:
        """Stop irrigation for a specific zone"""
        if zone_name not in self.zones:
            logger.error(f"Unknown zone: {zone_name}")
            return False
            
        return self.zones[zone_name].stop()
        
    def stop_all_zones(self):
        """Stop all zones"""
        for zone in self.zones.values():
            if zone.is_running:
                zone.stop("Stop all command")
                
    def get_all_status(self) -> Dict[str, Dict]:
        """Get status of all zones"""
        return {
            name: zone.get_status()
            for name, zone in self.zones.items()
        }


if __name__ == "__main__":
    # Example usage
    logging.basicConfig(level=logging.INFO)
    
    # Single zone
    pump = PumpController(relay_pin=5)
    
    print("Starting pump...")
    if pump.start("Test run"):
        time.sleep(5)
        print(f"Runtime: {pump.get_runtime()}s")
        pump.stop("Test complete")
        
    print(f"Status: {pump.get_status()}")
    print(f"Statistics: {pump.get_statistics()}")
    
    # Multi-zone
    multi = MultiZonePumpController({
        'front_yard': 5,
        'back_yard': 6,
        'garden': 7
    })
    
    print("\nMulti-zone status:")
    print(multi.get_all_status())
