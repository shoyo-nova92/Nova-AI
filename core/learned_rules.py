import json
import os
import re
import threading
from pathlib import Path
from datetime import datetime


class LearnedRules:
    """Loads and matches self-promoted fast-path rules learned from repeated
    complex_task successes. Data-driven (JSON), never executes generated code."""

    _instance = None
    _lock = threading.Lock()

    def __new__(cls, *args, **kwargs):
        with cls._lock:
            if cls._instance is None:
                cls._instance = super(LearnedRules, cls).__new__(cls)
                cls._instance._initialized = False
            return cls._instance

    def __init__(self):
        if getattr(self, "_initialized", False):
            return

        base_dir = os.path.dirname(os.path.abspath(__file__))
        base_parent = os.path.dirname(base_dir)
        self.rules_file = Path(os.path.join(base_parent, "memory", "learned_rules.json"))
        self.rules_file.parent.mkdir(parents=True, exist_ok=True)

        if not self.rules_file.exists():
            self._write({"rules": []})

        self._initialized = True

    def _read(self):
        try:
            with open(self.rules_file, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {"rules": []}

    def _write(self, data):
        temp_file = self.rules_file.with_suffix(".tmp")
        with open(temp_file, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=4)
        temp_file.replace(self.rules_file)

    def all_rules(self):
        return self._read().get("rules", [])

    def match(self, step):
        """Returns a TaskTranslator-shaped action dict if a learned rule matches,
        else None. Never raises on a malformed rule — skips it and continues."""
        if not step:
            return None

        for rule in self.all_rules():
            try:
                pattern = rule.get("pattern")
                action_template = rule.get("action")
                if not pattern or not action_template:
                    continue

                m = re.search(pattern, step, re.IGNORECASE)
                if not m:
                    continue

                action = dict(action_template)
                # allow {group_1}, {group_2}... substitution from regex captures
                target = action.get("target")
                if isinstance(target, str) and m.groups():
                    for i, group in enumerate(m.groups(), start=1):
                        target = target.replace(f"{{group_{i}}}", group or "")
                    action["target"] = target

                return action
            except Exception:
                continue

        return None

    def add_rule(self, rule: dict):
        """Validates minimal shape and appends. Returns True/False."""
        if not isinstance(rule, dict):
            return False
        if "pattern" not in rule or "action" not in rule:
            return False
        if not isinstance(rule["action"], dict):
            return False
        if "action" not in rule["action"] or "type" not in rule["action"]:
            return False

        # sanity-check the regex compiles before ever saving it
        try:
            re.compile(rule["pattern"])
        except re.error:
            return False

        data = self._read()
        rule.setdefault("id", f"rule_{len(data['rules']) + 1}")
        rule.setdefault("created_at", datetime.now().isoformat())
        data["rules"].append(rule)
        self._write(data)
        return True


# Module-level singleton accessors, mirroring the rest of core/*
_engine = LearnedRules()


def match(step):
    return _engine.match(step)


def add_rule(rule):
    return _engine.add_rule(rule)


def all_rules():
    return _engine.all_rules()
