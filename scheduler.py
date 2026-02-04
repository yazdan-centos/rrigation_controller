"""
Irrigation Scheduler - Advanced scheduling system
"""

import json
from datetime import datetime, time, timedelta
from typing import List, Dict, Optional
import logging

logger = logging.getLogger(__name__)


class ScheduleRule:
    """Represents a single irrigation schedule rule"""
    
    def __init__(self, rule_id: str, hour: int, minute: int, 
                 duration: int, days_of_week: List[int] = None,
                 enabled: bool = True):
        """
        Initialize schedule rule
        
        Args:
            rule_id: Unique identifier for the rule
            hour: Hour to run (0-23)
            minute: Minute to run (0-59)
            duration: Duration in seconds
            days_of_week: List of days (0=Monday, 6=Sunday), None=all days
            enabled: Whether the rule is active
        """
        self.rule_id = rule_id
        self.hour = hour
        self.minute = minute
        self.duration = duration
        self.days_of_week = days_of_week if days_of_week else list(range(7))
        self.enabled = enabled
        self.last_run = None
        
    def should_run(self, current_time: datetime) -> bool:
        """Check if the rule should run at the given time"""
        if not self.enabled:
            return False
            
        # Check day of week
        if current_time.weekday() not in self.days_of_week:
            return False
            
        # Check time
        if current_time.hour != self.hour or current_time.minute != self.minute:
            return False
            
        # Check if already run today
        if self.last_run:
            if self.last_run.date() == current_time.date():
                return False
                
        return True
        
    def mark_run(self, run_time: datetime):
        """Mark that the rule has been executed"""
        self.last_run = run_time
        
    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            'rule_id': self.rule_id,
            'hour': self.hour,
            'minute': self.minute,
            'duration': self.duration,
            'days_of_week': self.days_of_week,
            'enabled': self.enabled,
            'last_run': self.last_run.isoformat() if self.last_run else None
        }
        
    @classmethod
    def from_dict(cls, data: Dict) -> 'ScheduleRule':
        """Create from dictionary"""
        rule = cls(
            rule_id=data['rule_id'],
            hour=data['hour'],
            minute=data['minute'],
            duration=data['duration'],
            days_of_week=data.get('days_of_week'),
            enabled=data.get('enabled', True)
        )
        if data.get('last_run'):
            rule.last_run = datetime.fromisoformat(data['last_run'])
        return rule


class IrrigationScheduler:
    """Manages multiple irrigation schedules"""
    
    def __init__(self):
        self.rules: Dict[str, ScheduleRule] = {}
        
    def add_rule(self, rule: ScheduleRule):
        """Add a schedule rule"""
        self.rules[rule.rule_id] = rule
        logger.info(f"Added schedule rule: {rule.rule_id}")
        
    def remove_rule(self, rule_id: str):
        """Remove a schedule rule"""
        if rule_id in self.rules:
            del self.rules[rule_id]
            logger.info(f"Removed schedule rule: {rule_id}")
            
    def enable_rule(self, rule_id: str):
        """Enable a schedule rule"""
        if rule_id in self.rules:
            self.rules[rule_id].enabled = True
            logger.info(f"Enabled schedule rule: {rule_id}")
            
    def disable_rule(self, rule_id: str):
        """Disable a schedule rule"""
        if rule_id in self.rules:
            self.rules[rule_id].enabled = False
            logger.info(f"Disabled schedule rule: {rule_id}")
            
    def get_due_rules(self, current_time: datetime = None) -> List[ScheduleRule]:
        """Get all rules that should run now"""
        if current_time is None:
            current_time = datetime.now()
            
        due_rules = []
        for rule in self.rules.values():
            if rule.should_run(current_time):
                due_rules.append(rule)
                
        return due_rules
        
    def mark_rule_executed(self, rule_id: str, run_time: datetime = None):
        """Mark a rule as executed"""
        if rule_id in self.rules:
            if run_time is None:
                run_time = datetime.now()
            self.rules[rule_id].mark_run(run_time)
            logger.info(f"Marked rule {rule_id} as executed at {run_time}")
            
    def get_next_run(self, rule_id: str, from_time: datetime = None) -> Optional[datetime]:
        """Calculate next run time for a rule"""
        if rule_id not in self.rules:
            return None
            
        rule = self.rules[rule_id]
        if not rule.enabled:
            return None
            
        if from_time is None:
            from_time = datetime.now()
            
        # Find next occurrence
        next_run = from_time.replace(
            hour=rule.hour,
            minute=rule.minute,
            second=0,
            microsecond=0
        )
        
        # If time has passed today, start from tomorrow
        if next_run <= from_time:
            next_run += timedelta(days=1)
            
        # Find next day that matches days_of_week
        while next_run.weekday() not in rule.days_of_week:
            next_run += timedelta(days=1)
            
        return next_run
        
    def save_to_file(self, filename: str):
        """Save schedules to JSON file"""
        data = {
            'rules': [rule.to_dict() for rule in self.rules.values()]
        }
        with open(filename, 'w') as f:
            json.dump(data, f, indent=2)
        logger.info(f"Saved schedules to {filename}")
        
    def load_from_file(self, filename: str):
        """Load schedules from JSON file"""
        try:
            with open(filename, 'r') as f:
                data = json.load(f)
            
            self.rules.clear()
            for rule_data in data.get('rules', []):
                rule = ScheduleRule.from_dict(rule_data)
                self.rules[rule.rule_id] = rule
                
            logger.info(f"Loaded {len(self.rules)} schedules from {filename}")
        except FileNotFoundError:
            logger.warning(f"Schedule file {filename} not found")
        except Exception as e:
            logger.error(f"Error loading schedules: {e}")
            
    def get_all_rules(self) -> List[ScheduleRule]:
        """Get all schedule rules"""
        return list(self.rules.values())


def create_default_schedule() -> IrrigationScheduler:
    """Create a default schedule configuration"""
    scheduler = IrrigationScheduler()
    
    # Morning schedule - every day at 6:00 AM for 5 minutes
    scheduler.add_rule(ScheduleRule(
        rule_id="morning",
        hour=6,
        minute=0,
        duration=300,
        days_of_week=list(range(7))
    ))
    
    # Evening schedule - every day at 6:00 PM for 5 minutes
    scheduler.add_rule(ScheduleRule(
        rule_id="evening",
        hour=18,
        minute=0,
        duration=300,
        days_of_week=list(range(7))
    ))
    
    return scheduler


if __name__ == "__main__":
    # Example usage
    scheduler = create_default_schedule()
    
    # Check what should run now
    due_rules = scheduler.get_due_rules()
    print(f"Rules due now: {[r.rule_id for r in due_rules]}")
    
    # Get next run time
    for rule in scheduler.get_all_rules():
        next_run = scheduler.get_next_run(rule.rule_id)
        print(f"{rule.rule_id}: next run at {next_run}")
