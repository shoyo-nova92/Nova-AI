import json
import os
from collections import defaultdict
from pathlib import Path

from core.learned_rules import LearnedRules
from core.llm_client import LLMClient
from core.llm_json import extract_json_object


class PromotionEngine:
    """Promote repeatable, verified complex tasks into reviewed JSON rules."""

    def __init__(self, min_occurrences=3, llm_client=None, rules_engine=None, approval_callback=None):
        base_dir = os.path.dirname(os.path.abspath(__file__))
        base_parent = os.path.dirname(base_dir)
        self.task_logs_file = Path(os.path.join(base_parent, "memory", "task_logs.json"))
        self.candidates_file = Path(os.path.join(base_parent, "memory", "promotion_candidates.json"))
        self.min_occurrences = min_occurrences
        self.llm_client = llm_client or LLMClient()
        self.rules_engine = rules_engine or LearnedRules()
        self.approval_callback = approval_callback or self._request_approval

    def _load_task_logs(self):
        if not self.task_logs_file.exists():
            return []
        try:
            with open(self.task_logs_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            return data if isinstance(data, list) else []
        except Exception as exc:
            print(f"[PROMOTION] Could not read task logs: {exc}")
            return []

    def _load_candidate_history(self):
        if not self.candidates_file.exists():
            return []
        try:
            with open(self.candidates_file, "r", encoding="utf-8") as f:
                data = json.load(f)
            return data if isinstance(data, list) else []
        except Exception:
            return []

    def find_candidates(self):
        """Return repeated successful complex goals not already declined or learned."""
        logs = self._load_task_logs()
        rejected_goals = {
            item.get("normalized_goal")
            for item in self._load_candidate_history()
            if item.get("status") == "rejected"
        }
        grouped = defaultdict(lambda: {"occurrences": 0, "successes": 0, "task_ids": []})

        for entry in logs:
            if entry.get("intent_category") != "complex_task":
                continue
            raw_input = (entry.get("raw_input") or "").strip().lower()
            if not raw_input:
                continue
            group = grouped[raw_input]
            group["occurrences"] += 1
            if entry.get("success"):
                group["successes"] += 1
            group["task_ids"].append(entry.get("task_id"))

        candidates = []
        for normalized_goal, stats in grouped.items():
            if stats["occurrences"] < self.min_occurrences:
                continue
            if stats["successes"] != stats["occurrences"]:
                continue
            if normalized_goal in rejected_goals:
                continue
            if self.rules_engine.match(normalized_goal) is not None:
                continue
            candidates.append({
                "normalized_goal": normalized_goal,
                "occurrences": stats["occurrences"],
                "success_count": stats["successes"],
                "sample_task_ids": stats["task_ids"][-3:],
                "status": "pending_generation",
            })
        return candidates

    def _ground_truth(self, candidate):
        sample_ids = set(candidate.get("sample_task_ids") or [])
        traces = []
        for entry in self._load_task_logs():
            if entry.get("task_id") not in sample_ids:
                continue
            runtime_result = (entry.get("metadata") or {}).get("runtime_result") or {}
            executions = runtime_result.get("executions") or entry.get("executions") or []
            action_pairs = []
            for execution in executions:
                action = execution.get("action") if isinstance(execution, dict) else None
                if isinstance(action, dict) and action.get("action"):
                    action_pairs.append({
                        "type": action.get("type"),
                        "action": action.get("action"),
                        "action_type": action.get("action_type") or action.get("action"),
                        "target": action.get("target"),
                    })
            traces.append({
                "task_id": entry.get("task_id"),
                "actions": action_pairs,
                "flowchart_steps": entry.get("flowchart_steps") or [],
                "self_building_data": entry.get("self_building_data") or {},
            })
        return traces

    def generate_rule(self, candidate):
        """Ask the LLM for a declarative rule based only on successful traces."""
        traces = self._ground_truth(candidate)
        action_pairs = [action for trace in traces for action in trace["actions"]]
        if not action_pairs:
            print(f"[PROMOTION] No executable ground truth for {candidate.get('normalized_goal')!r}.")
            return None

        prompt = (
            "Create one safe deterministic Nova fast-path rule from successful execution traces.\n"
            f"Normalized goal: {candidate.get('normalized_goal')}\n"
            f"Executed action evidence: {json.dumps(action_pairs, indent=2)}\n"
            f"Supporting trace metadata: {json.dumps(traces, indent=2)}\n\n"
            "Return JSON only, with exactly this schema:\n"
            '{"pattern":"Python regex", "action":{"type":"...", "action":"...", '
            '"action_type":"...", "target":"..."}}\n'
            "Use only an action shown in the evidence. The pattern should be loosely anchored and "
            "may use {group_1} placeholders in target for regex captures. Never emit code."
        )
        try:
            raw = self.llm_client.generate(prompt=prompt)
        except Exception as exc:
            print(f"[PROMOTION] Rule generation request failed: {exc}")
            return None

        rule = extract_json_object(raw)
        if not isinstance(rule, dict) or not isinstance(rule.get("action"), dict) or not rule.get("pattern"):
            print(f"[PROMOTION] Rule generation returned invalid JSON/schema for {candidate.get('normalized_goal')!r}.")
            return None
        return rule

    @staticmethod
    def _request_approval(candidate, rule):
        print("\n[PROMOTION] Candidate rule awaiting approval")
        print(f"Goal: {candidate.get('normalized_goal')}")
        print(f"Pattern: {rule.get('pattern')}")
        print(f"Action: {json.dumps(rule.get('action'), indent=2)}")
        try:
            answer = input("Add this learned rule? (yes/no): ")
        except (EOFError, KeyboardInterrupt):
            return False
        return answer.strip().lower() in {"yes", "y"}

    def _record_decision(self, candidate, status, rule=None, reason=None):
        history = self._load_candidate_history()
        goal = candidate.get("normalized_goal")
        history = [item for item in history if item.get("normalized_goal") != goal]
        record = dict(candidate)
        record["status"] = status
        if rule is not None:
            record["rule"] = rule
        if reason:
            record["reason"] = reason
        history.append(record)
        self.candidates_file.parent.mkdir(parents=True, exist_ok=True)
        with open(self.candidates_file, "w", encoding="utf-8") as f:
            json.dump(history, f, indent=4)

    def promote(self, candidate):
        rule = self.generate_rule(candidate)
        if rule is None:
            self._record_decision(candidate, "generation_failed", reason="LLM rule generation failed")
            return {"promoted": False, "reason": "rule generation failed"}

        if not self.approval_callback(candidate, rule):
            self._record_decision(candidate, "rejected", rule=rule, reason="user rejected rule")
            print(f"[PROMOTION] Rule rejected for {candidate.get('normalized_goal')!r}.")
            return {"promoted": False, "reason": "rule rejected", "rule": rule}

        added = self.rules_engine.add_rule(rule)
        if added:
            self._record_decision(candidate, "approved", rule=rule)
            print(f"[PROMOTION] Rule approved and stored for {candidate.get('normalized_goal')!r}.")
            return {"promoted": True, "rule": rule}

        self._record_decision(candidate, "generation_failed", rule=rule, reason="rule failed LearnedRules validation")
        print(f"[PROMOTION] Generated rule failed validation for {candidate.get('normalized_goal')!r}.")
        return {"promoted": False, "reason": "rule validation failed", "rule": rule}

    def save_candidates(self, candidates):
        """Refresh pending candidates without erasing prior approval decisions."""
        history = {item.get("normalized_goal"): item for item in self._load_candidate_history()}
        for candidate in candidates:
            previous = history.get(candidate.get("normalized_goal"), {})
            if previous.get("status") in {"approved", "rejected"}:
                continue
            history[candidate.get("normalized_goal")] = candidate
        self.candidates_file.parent.mkdir(parents=True, exist_ok=True)
        with open(self.candidates_file, "w", encoding="utf-8") as f:
            json.dump(list(history.values()), f, indent=4)

    def run_scan(self):
        candidates = self.find_candidates()
        self.save_candidates(candidates)
        for candidate in candidates:
            self.promote(candidate)
        return candidates
