import json
import os
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional


class SelfLogger:
    """Centralized self-logging and self-reflection engine for Nova.

    Captures every command and task across all execution pathways, generating:
      1. Process Flowchart: Step-by-step visual trace of execution
      2. Crux Analysis: Root cause explanation of why it succeeded or failed
      3. What Went Right vs What Went Wrong breakdown
      4. Self-Building Intelligence: Continuous learning data saved to memory/
    """

    _instance = None
    _lock = threading.Lock()

    def __new__(cls, *args, **kwargs):
        with cls._lock:
            if cls._instance is None:
                cls._instance = super(SelfLogger, cls).__new__(cls)
                cls._instance._initialized = False
            return cls._instance

    def __init__(self, memory_dir: Optional[str] = None):
        if getattr(self, "_initialized", False):
            return

        base_dir = os.path.dirname(os.path.abspath(__file__))
        base_parent = os.path.dirname(base_dir)

        if memory_dir:
            self.memory_dir = Path(memory_dir)
        else:
            self.memory_dir = Path(os.path.join(base_parent, "memory"))

        self.memory_dir.mkdir(parents=True, exist_ok=True)

        self.task_logs_file = self.memory_dir / "task_logs.json"
        self.insights_file = self.memory_dir / "self_learning_insights.json"
        self.execution_history_file = self.memory_dir / "execution_history.json"
        self.runtime_history_file = self.memory_dir / "runtime_history.json"
        self.skills_file = self.memory_dir / "skills.json"

        self._file_lock = threading.Lock()
        self._ensure_files()
        self._initialized = True

    def _ensure_files(self):
        """Ensure all required memory files exist with valid JSON structure."""
        now_local = datetime.now()
        now_utc = datetime.now(timezone.utc)
        if not self.task_logs_file.exists():
            self._write_json(self.task_logs_file, [])

        if not self.insights_file.exists():
            self._write_json(
                self.insights_file,
                {
                    "created_at": now_utc.isoformat(),
                    "created_day": now_local.strftime("%A"),
                    "created_date": now_local.strftime("%Y-%m-%d"),
                    "created_time": now_local.strftime("%H:%M:%S"),
                    "last_updated": now_utc.isoformat(),
                    "last_updated_day": now_local.strftime("%A"),
                    "last_updated_date": now_local.strftime("%Y-%m-%d"),
                    "last_updated_time": now_local.strftime("%H:%M:%S"),
                    "total_tasks_logged": 0,
                    "success_count": 0,
                    "failure_count": 0,
                    "learned_aliases": {},
                    "frequent_commands": {},
                    "failure_patterns": {},
                    "recommendations": [],
                },
            )

        if not self.execution_history_file.exists():
            self._write_json(self.execution_history_file, [])

        if not self.runtime_history_file.exists():
            self._write_json(self.runtime_history_file, [])

        if not self.skills_file.exists():
            self._write_json(self.skills_file, {})

    def reset_all_logs(self):
        """Wipe all logs and re-initialize clean memory files with proper indentation."""
        now_local = datetime.now()
        now_utc = datetime.now(timezone.utc)
        self._write_json(self.task_logs_file, [])
        self._write_json(self.execution_history_file, [])
        self._write_json(self.runtime_history_file, [])
        self._write_json(self.skills_file, {})
        self._write_json(
            self.insights_file,
            {
                "created_at": now_utc.isoformat(),
                "created_day": now_local.strftime("%A"),
                "created_date": now_local.strftime("%Y-%m-%d"),
                "created_time": now_local.strftime("%H:%M:%S"),
                "last_updated": now_utc.isoformat(),
                "last_updated_day": now_local.strftime("%A"),
                "last_updated_date": now_local.strftime("%Y-%m-%d"),
                "last_updated_time": now_local.strftime("%H:%M:%S"),
                "total_tasks_logged": 0,
                "success_count": 0,
                "failure_count": 0,
                "learned_aliases": {},
                "frequent_commands": {},
                "failure_patterns": {},
                "recommendations": [],
            },
        )
        session_file = self.memory_dir / "input_layer_session.json"
        if session_file.exists():
            try:
                session_file.unlink()
            except Exception:
                pass
        traces_file = self.memory_dir / "runtime_traces.json"
        if traces_file.exists():
            try:
                traces_file.unlink()
            except Exception:
                pass

    def _read_json(self, file_path: Path, default_val: Any) -> Any:
        with self._file_lock:
            if not file_path.exists():
                return default_val
            try:
                with open(file_path, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception:
                return default_val

    def _write_json(self, file_path: Path, data: Any):
        with self._file_lock:
            try:
                temp_file = file_path.with_suffix(".tmp")
                with open(temp_file, "w", encoding="utf-8") as f:
                    json.dump(data, f, indent=4, default=str)
                temp_file.replace(file_path)
            except Exception:
                # Fallback direct write
                try:
                    with open(file_path, "w", encoding="utf-8") as f:
                        json.dump(data, f, indent=4, default=str)
                except Exception:
                    pass

    # ----------------------------------------------------------------------
    # Flowchart Generation
    # ----------------------------------------------------------------------

    @staticmethod
    def generate_ascii_flowchart(
        steps: List[Dict[str, str]],
        status: str,
        command: Optional[str] = None,
        timestamp_display: Optional[str] = None,
    ) -> str:
        """Render a clean ASCII flowchart from ordered lifecycle steps with date, time, and day."""
        lines = []
        width = 68

        if timestamp_display or command:
            lines.append("+" + "-" * (width - 2) + "+")
            if timestamp_display:
                lines.append(f"| [TIMESTAMP] {timestamp_display:<{width - 16}} |")
            if command:
                clean_cmd = f"Command: '{command}'"
                if len(clean_cmd) > width - 4:
                    clean_cmd = clean_cmd[: width - 7] + "..."
                lines.append(f"| {clean_cmd:<{width - 4}} |")
            lines.append("+" + "-" * (width - 2) + "+")
            lines.append(f"{'|':^{width}}")
            lines.append(f"{'v':^{width}}")

        for i, step in enumerate(steps, start=1):
            stage = step.get("stage", "Step")
            comp = step.get("component", "Nova")
            detail = step.get("detail", "")
            step_status = step.get("status", "OK")

            badge = "[OK]" if step_status.upper() in {"OK", "SUCCESS"} else "[FAIL]"
            header = f"{badge} {i}. {stage} ({comp})"

            # Truncate detail cleanly if too long
            max_detail_len = width - 8
            if len(detail) > max_detail_len:
                detail_text = detail[: max_detail_len - 3] + "..."
            else:
                detail_text = detail

            lines.append("+" + "-" * (width - 2) + "+")
            lines.append(f"| {header:<{width - 4}} |")
            if detail_text:
                lines.append(f"|   -> {detail_text:<{width - 7}} |")
            lines.append("+" + "-" * (width - 2) + "+")

            if i < len(steps):
                lines.append(f"{'|':^{width}}")
                lines.append(f"{'v':^{width}}")

        outcome_badge = ">>> OUTCOME: SUCCESS <<<" if status.upper() == "SUCCESS" else ">>> OUTCOME: FAILED <<<"
        lines.append(f"{'|':^{width}}")
        lines.append(f"{'v':^{width}}")
        lines.append("=" * width)
        lines.append(f"{outcome_badge:^{width}}")
        lines.append("=" * width)

        return "\n".join(lines)

    @staticmethod
    def generate_mermaid_flowchart(steps: List[Dict[str, str]], status: str) -> str:
        """Generate a Mermaid diagram for documentation or UI rendering."""
        mermaid_lines = ["graph TD"]
        for i, step in enumerate(steps):
            node_id = f"S{i+1}"
            stage = step.get("stage", f"Step {i+1}").replace('"', "'")
            detail = step.get("detail", "").replace('"', "'")
            label = f'"{stage}: {detail}"' if detail else f'"{stage}"'
            mermaid_lines.append(f"    {node_id}[{label}]")

            if i > 0:
                prev_id = f"S{i}"
                mermaid_lines.append(f"    {prev_id} --> {node_id}")

        outcome_id = "Outcome"
        mermaid_lines.append(f'    {outcome_id}{{"Outcome: {status}"}}')
        if steps:
            mermaid_lines.append(f"    S{len(steps)} --> {outcome_id}")

        return "\n".join(mermaid_lines)

    # ----------------------------------------------------------------------
    # Logging Methods
    # ----------------------------------------------------------------------

    def log_task(
        self,
        raw_input: str,
        intent_category: str,
        dispatched_branch: str,
        status: str,
        duration_seconds: float,
        flowchart_steps: List[Dict[str, str]],
        what_went_right: List[str],
        what_went_wrong: List[str],
        crux: str,
        self_building_data: Optional[Dict[str, Any]] = None,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Record a comprehensive self-logging entry for any executed task."""
        now_local = datetime.now()
        now_utc = datetime.now(timezone.utc)

        day_str = now_local.strftime("%A")
        date_str = now_local.strftime("%Y-%m-%d")
        time_str = now_local.strftime("%H:%M:%S")
        datetime_str = now_local.strftime("%Y-%m-%d %H:%M:%S")
        datetime_formatted = f"{day_str}, {date_str} {time_str}"
        timestamp_str = now_utc.isoformat()
        task_id = f"task_{now_local.strftime('%Y%m%d_%H%M%S')}_{os.urandom(2).hex()}"

        normalized_status = status.upper()
        ascii_flowchart = self.generate_ascii_flowchart(
            steps=flowchart_steps,
            status=normalized_status,
            command=raw_input,
            timestamp_display=datetime_formatted,
        )
        mermaid_flowchart = self.generate_mermaid_flowchart(flowchart_steps, normalized_status)

        entry = {
            "task_id": task_id,
            "day": day_str,
            "date": date_str,
            "time": time_str,
            "datetime": datetime_str,
            "datetime_formatted": datetime_formatted,
            "timestamp": timestamp_str,
            "raw_input": raw_input,
            "intent_category": intent_category,
            "dispatched_branch": dispatched_branch,
            "status": normalized_status,
            "success": normalized_status == "SUCCESS",
            "duration_seconds": round(duration_seconds, 3),
            "flowchart_steps": flowchart_steps,
            "flowchart_ascii": ascii_flowchart,
            "flowchart_mermaid": mermaid_flowchart,
            "what_went_right": what_went_right or [],
            "what_went_wrong": what_went_wrong or [],
            "crux": crux,
            "self_building_data": self_building_data or {},
            "metadata": metadata or {},
        }

        # 1. Append to task_logs.json
        tasks = self._read_json(self.task_logs_file, [])
        tasks.append(entry)
        # Keep last 1000 tasks to prevent unbounded growth
        if len(tasks) > 1000:
            tasks = tasks[-1000:]
        self._write_json(self.task_logs_file, tasks)

        # 2. Update self_learning_insights.json
        self._update_insights(entry)

        # 3. Synchronize with legacy execution_history.json & runtime_history.json
        self._sync_legacy(entry)

        return entry

    def _update_insights(self, entry: Dict[str, Any]):
        """Derive continuous learning insights from the new task entry."""
        now_local = datetime.now()
        now_utc = datetime.now(timezone.utc)
        insights = self._read_json(
            self.insights_file,
            {
                "created_at": now_utc.isoformat(),
                "created_day": now_local.strftime("%A"),
                "created_date": now_local.strftime("%Y-%m-%d"),
                "created_time": now_local.strftime("%H:%M:%S"),
                "last_updated": now_utc.isoformat(),
                "last_updated_day": now_local.strftime("%A"),
                "last_updated_date": now_local.strftime("%Y-%m-%d"),
                "last_updated_time": now_local.strftime("%H:%M:%S"),
                "total_tasks_logged": 0,
                "success_count": 0,
                "failure_count": 0,
                "learned_aliases": {},
                "frequent_commands": {},
                "failure_patterns": {},
                "recommendations": [],
            },
        )

        insights["last_updated"] = now_utc.isoformat()
        insights["last_updated_day"] = now_local.strftime("%A")
        insights["last_updated_date"] = now_local.strftime("%Y-%m-%d")
        insights["last_updated_time"] = now_local.strftime("%H:%M:%S")
        insights["total_tasks_logged"] = insights.get("total_tasks_logged", 0) + 1

        if entry.get("success"):
            insights["success_count"] = insights.get("success_count", 0) + 1
        else:
            insights["failure_count"] = insights.get("failure_count", 0) + 1

        # Track command frequency
        raw_cmd = (entry.get("raw_input") or "").strip().lower()
        if raw_cmd:
            cmd_stats = insights.setdefault("frequent_commands", {}).setdefault(
                raw_cmd, {"count": 0, "successes": 0, "failures": 0}
            )
            cmd_stats["count"] += 1
            if entry.get("success"):
                cmd_stats["successes"] += 1
            else:
                cmd_stats["failures"] += 1

        # Track learned aliases & typos
        self_b = entry.get("self_building_data") or {}
        aliases = self_b.get("learned_aliases") or {}
        for alias_key, target_val in aliases.items():
            alias_store = insights.setdefault("learned_aliases", {})
            if alias_key not in alias_store:
                alias_store[alias_key] = {
                    "resolved_target": target_val,
                    "frequency": 1,
                    "confidence": self_b.get("confidence_score", 100.0),
                    "last_used": entry.get("timestamp"),
                }
            else:
                alias_store[alias_key]["frequency"] += 1
                alias_store[alias_key]["last_used"] = entry.get("timestamp")

        # Track failure patterns
        if not entry.get("success"):
            err_cat = self_b.get("error_category") or "UNKNOWN_ERROR"
            pattern_store = insights.setdefault("failure_patterns", {})
            err_stat = pattern_store.setdefault(
                err_cat, {"occurrences": 0, "sample_crux": entry.get("crux"), "sample_command": raw_cmd}
            )
            err_stat["occurrences"] += 1
            err_stat["sample_crux"] = entry.get("crux")

        # Dynamic recommendations
        recs = []
        fail_count = insights.get("failure_count", 0)
        total = insights.get("total_tasks_logged", 1)
        if fail_count > 0 and (fail_count / total) > 0.2:
            recs.append("Failure rate is >20%. Check missing system paths or installed applications.")

        for alias_key, alias_info in insights.get("learned_aliases", {}).items():
            if alias_info.get("frequency", 0) >= 2:
                recs.append(
                    f"Frequent user typo/alias detected: '{alias_key}' -> '{alias_info.get('resolved_target')}'. Caching for instant lookup."
                )

        if "APP_NOT_FOUND" in insights.get("failure_patterns", {}):
            recs.append("Applications frequently missing. Suggest re-indexing Start Menu & Registry.")

        insights["recommendations"] = list(dict.fromkeys(recs))[:10]
        self._write_json(self.insights_file, insights)

    def _sync_legacy(self, entry: Dict[str, Any]):
        """Maintain full backward compatibility with existing tests & modules."""
        action_label = entry.get("raw_input", "unknown")
        success = entry.get("success", False)
        duration = entry.get("duration_seconds", 0.0)
        failure_reason = None if success else entry.get("crux")

        now_local = datetime.now()
        day_str = entry.get("day") or now_local.strftime("%A")
        date_str = entry.get("date") or now_local.strftime("%Y-%m-%d")
        time_str = entry.get("time") or now_local.strftime("%H:%M:%S")

        # Sync execution_history.json
        legacy_exec_entry = {
            "day": day_str,
            "date": date_str,
            "time": time_str,
            "timestamp": str(now_local),
            "action": action_label,
            "success": success,
            "duration": duration,
            "failure_reason": failure_reason,
        }
        exec_hist = self._read_json(self.execution_history_file, [])
        exec_hist.append(legacy_exec_entry)
        if len(exec_hist) > 1000:
            exec_hist = exec_hist[-1000:]
        self._write_json(self.execution_history_file, exec_hist)

        # Sync runtime_history.json
        runtime_hist = self._read_json(self.runtime_history_file, [])
        event_id = len(runtime_hist) + 1
        legacy_runtime_entry = {
            "event_id": event_id,
            "day": day_str,
            "date": date_str,
            "time": time_str,
            "timestamp": str(now_local),
            "event_type": "execution_success" if success else "execution_failure",
            "goal": action_label,
            "details": {
                "duration": duration,
                "state": "complete" if success else "failed",
                "crux": entry.get("crux"),
            },
        }
        runtime_hist.append(legacy_runtime_entry)
        if len(runtime_hist) > 1000:
            runtime_hist = runtime_hist[-1000:]
        self._write_json(self.runtime_history_file, runtime_hist)

        # Sync skills.json
        skill_name = entry.get("intent_category", "action")
        self_b = entry.get("self_building_data") or {}
        target = self_b.get("target_app") or self_b.get("target")
        if target:
            skill_name = f"{skill_name}:{target}"

        skills = self._read_json(self.skills_file, {})
        if skill_name not in skills:
            skills[skill_name] = {"usage_count": 0, "success_count": 0}
        skills[skill_name]["usage_count"] += 1
        if success:
            skills[skill_name]["success_count"] += 1
        self._write_json(self.skills_file, skills)

    # ----------------------------------------------------------------------
    # Convenience Loggers for Distinct Pathways
    # ----------------------------------------------------------------------

    def log_entrygate_execution(
        self,
        raw_command: str,
        entrygate_result: Dict[str, Any],
        duration_seconds: float,
    ) -> Dict[str, Any]:
        """Specialized logger for RuntimeEntrygate actions (e.g. open chrome, close app, exit)."""
        action = entrygate_result.get("action", "unknown")
        target = entrygate_result.get("target") or ""
        success = entrygate_result.get("success", False)
        result_details = entrygate_result.get("result") or {}

        flowchart_steps = [
            {
                "stage": "Intake & Normalization",
                "component": "InputNormalizer",
                "detail": f"Received command: '{raw_command}'",
                "status": "OK",
            },
            {
                "stage": "Gate Classification",
                "component": "RuntimeEntrygate",
                "detail": f"Classified as '{action}' with target: '{target}'",
                "status": "OK",
            },
        ]

        what_went_right = ["Command syntax successfully parsed by RuntimeEntrygate."]
        what_went_wrong = []
        self_building_data = {
            "action": action,
            "target": target,
            "learned_aliases": {},
            "error_category": None,
            "confidence_score": 100.0,
        }

        if action == "open":
            resolved_app = result_details.get("app_name") or target
            confidence = float(result_details.get("confidence") or 0.0)
            self_building_data["target_app"] = resolved_app
            self_building_data["confidence_score"] = confidence

            is_typo_correction = (
                target.lower() != resolved_app.lower()
                and target.lower() not in resolved_app.lower()
                and success
            ) or (confidence > 0 and confidence < 95 and success)

            if is_typo_correction:
                self_building_data["learned_aliases"][target.lower()] = resolved_app.lower()
                flowchart_steps.append(
                    {
                        "stage": "Fuzzy App Resolution",
                        "component": "AppSearchEngine",
                        "detail": f"Corrected typo '{target}' -> '{resolved_app}' (Confidence: {confidence:.1f}%)",
                        "status": "OK",
                    }
                )
                what_went_right.append(
                    f"Fuzzy search successfully resolved typo '{target}' to installed app '{resolved_app}' ({confidence:.1f}% confidence)."
                )
                what_went_wrong.append(f"Input contained misspelled target: '{target}'.")
            elif success:
                flowchart_steps.append(
                    {
                        "stage": "App Resolution",
                        "component": "AppSearchEngine",
                        "detail": f"Resolved '{target}' to '{resolved_app}'",
                        "status": "OK",
                    }
                )
                what_went_right.append(f"Located executable for '{resolved_app}' via Start Menu/Registry.")
            else:
                flowchart_steps.append(
                    {
                        "stage": "App Resolution",
                        "component": "AppSearchEngine",
                        "detail": f"Could not locate application '{target}'",
                        "status": "FAILED",
                    }
                )
                what_went_wrong.append(
                    result_details.get("reason") or f"Application '{target}' not found in installed apps or PATH."
                )

            if success:
                flowchart_steps.append(
                    {
                        "stage": "Process Execution",
                        "component": "ApplicationHandler",
                        "detail": f"Launched '{resolved_app}' ({result_details.get('via', 'exe')})",
                        "status": "OK",
                    }
                )
                flowchart_steps.append(
                    {
                        "stage": "Verification",
                        "component": "ExecutionVerifier",
                        "detail": "Application launch completed without system errors",
                        "status": "OK",
                    }
                )
                what_went_right.append(f"Successfully spawned process for '{resolved_app}'.")
                if is_typo_correction:
                    crux = f"Execution succeeded with auto-correction: Nova recognized '{target}' as a typo for '{resolved_app}' and launched it successfully."
                else:
                    crux = f"Execution succeeded: Nova verified and launched '{resolved_app}' cleanly."
            else:
                self_building_data["error_category"] = "APP_NOT_FOUND"
                crux = f"Execution failed: Application '{target}' is not installed or not registered in the system PATH/Start Menu."

        elif action == "close":
            if success:
                flowchart_steps.append(
                    {
                        "stage": "Process Termination",
                        "component": "AppSearchEngine",
                        "detail": f"Terminated process matching '{target}'",
                        "status": "OK",
                    }
                )
                what_went_right.append(f"Closed process '{target}'.")
                crux = f"Execution succeeded: Nova terminated running process '{target}'."
            else:
                flowchart_steps.append(
                    {
                        "stage": "Process Termination",
                        "component": "AppSearchEngine",
                        "detail": f"No running process matching '{target}' found",
                        "status": "FAILED",
                    }
                )
                what_went_wrong.append(f"Target process '{target}' was not currently running.")
                self_building_data["error_category"] = "PROCESS_NOT_RUNNING"
                crux = f"Execution failed: Could not close '{target}' because no active matching process was found."

        elif action in {"conversation", "exit"}:
            flowchart_steps.append(
                {
                    "stage": "Response Handler",
                    "component": "ConversationHandler",
                    "detail": f"Handled {action} query",
                    "status": "OK",
                }
            )
            what_went_right.append(f"Handled {action} request immediately.")
            crux = f"Execution succeeded: Handled {action} intent directly without cognitive overhead."

        else:
            flowchart_steps.append(
                {
                    "stage": "Execution",
                    "component": "RuntimeEntrygate",
                    "detail": f"Dispatched '{action}'",
                    "status": "OK" if success else "FAILED",
                }
            )
            crux = f"Handled {action} command."

        return self.log_task(
            raw_input=raw_command,
            intent_category=action,
            dispatched_branch=entrygate_result.get("branch", "entrygate"),
            status="SUCCESS" if success else "FAILED",
            duration_seconds=duration_seconds,
            flowchart_steps=flowchart_steps,
            what_went_right=what_went_right,
            what_went_wrong=what_went_wrong,
            crux=crux,
            self_building_data=self_building_data,
            metadata={"entrygate": entrygate_result},
        )

    def log_conversational_task(
        self,
        raw_command: str,
        response_text: str,
        duration_seconds: float,
        router_result: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Specialized logger for informational / conversational queries."""
        flowchart_steps = [
            {
                "stage": "Intake",
                "component": "InputNormalizer",
                "detail": f"Received: '{raw_command}'",
                "status": "OK",
            },
            {
                "stage": "Intent Routing",
                "component": "InputRouter",
                "detail": "Classified as conversational query",
                "status": "OK",
            },
            {
                "stage": "LLM Generation",
                "component": "ConversationalRuntime",
                "detail": f"Generated {len(response_text)} chars response",
                "status": "OK",
            },
        ]

        what_went_right = ["User intent classified as conversational.", "LLM generated a contextual response."]
        crux = f"Conversational query answered successfully: {response_text[:80]}..."

        return self.log_task(
            raw_input=raw_command,
            intent_category="conversational",
            dispatched_branch="input_router_conversational",
            status="SUCCESS",
            duration_seconds=duration_seconds,
            flowchart_steps=flowchart_steps,
            what_went_right=what_went_right,
            what_went_wrong=[],
            crux=crux,
            self_building_data={"response_length": len(response_text)},
            metadata={"router": router_result},
        )

    def log_complex_task(
        self,
        goal: str,
        runtime_result: Dict[str, Any],
        duration_seconds: float,
        router_result: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Specialized logger for multi-step tasks orchestrated by NovaRuntime."""
        status = runtime_result.get("status", "COMPLETED")
        is_success = status in {"COMPLETED", "RECOVERED"}
        executions = runtime_result.get("executions") or []
        verification = runtime_result.get("verification") or {}
        recovery = runtime_result.get("recovery")

        flowchart_steps = [
            {
                "stage": "Intake & Context Fusion",
                "component": "NovaRuntime.observe & build_context",
                "detail": f"Captured desktop context for goal: '{goal}'",
                "status": "OK",
            },
            {
                "stage": "Planner & Pipeline",
                "component": "PlannerPipeline",
                "detail": f"Plan formulated ({len(runtime_result.get('validated_plan') or runtime_result.get('raw_plan') or [])} steps)",
                "status": "OK",
            },
        ]

        what_went_right = ["Environment observations gathered.", "Multi-step plan constructed and validated."]
        what_went_wrong = []

        for i, exec_entry in enumerate(executions):
            action_data = exec_entry.get("action") or {}
            action_str = action_data.get("action") if isinstance(action_data, dict) else str(action_data)
            action_target = action_data.get("target", "") if isinstance(action_data, dict) else ""
            res = exec_entry.get("result") or {}
            step_ok = res.get("success", False) if isinstance(res, dict) else False

            flowchart_steps.append(
                {
                    "stage": f"Execute Step {i+1}",
                    "component": "ExecutionRouter",
                    "detail": f"{action_str} {action_target}".strip(),
                    "status": "OK" if step_ok else "FAILED",
                }
            )

            if step_ok:
                what_went_right.append(f"Step {i+1} ({action_str} {action_target}) executed successfully.")
            else:
                reason = res.get("reason", "Step failed") if isinstance(res, dict) else "Unknown failure"
                what_went_wrong.append(f"Step {i+1} ({action_str}) failed: {reason}")

        v_ok = verification.get("success", is_success)
        flowchart_steps.append(
            {
                "stage": "Verification",
                "component": "ExecutionVerifier",
                "detail": verification.get("reason", "All steps verified"),
                "status": "OK" if v_ok else "FAILED",
            }
        )
        if not v_ok:
            what_went_wrong.append(f"Verification failure: {verification.get('reason')}")

        if recovery:
            rec_ok = recovery.get("recovered", False)
            flowchart_steps.append(
                {
                    "stage": "Self-Healing Recovery",
                    "component": "RecoveryEngine",
                    "detail": f"Recovery action attempted (Recovered: {rec_ok})",
                    "status": "OK" if rec_ok else "FAILED",
                }
            )
            if rec_ok:
                what_went_right.append("Self-healing recovery successfully corrected the execution error.")
            else:
                what_went_wrong.append("Self-healing recovery could not salvage execution.")

        if is_success:
            if recovery and recovery.get("recovered"):
                crux = f"Execution recovered: Goal '{goal}' encountered verification failure but was healed by RecoveryEngine."
            else:
                crux = f"Execution succeeded: All {len(executions)} steps for '{goal}' executed and verified cleanly."
        else:
            crux = f"Execution failed for '{goal}': {verification.get('reason') or (what_went_wrong[0] if what_went_wrong else 'Execution halted')}"

        self_building_data = {
            "total_steps": len(executions),
            "recovered": bool(recovery and recovery.get("recovered")),
            "error_category": "VERIFICATION_ERROR" if not v_ok else None,
        }

        return self.log_task(
            raw_input=goal,
            intent_category="complex_task",
            dispatched_branch="nova_runtime_complex",
            status="SUCCESS" if is_success else "FAILED",
            duration_seconds=duration_seconds,
            flowchart_steps=flowchart_steps,
            what_went_right=what_went_right,
            what_went_wrong=what_went_wrong,
            crux=crux,
            self_building_data=self_building_data,
            metadata={"runtime_result": runtime_result, "router": router_result},
        )

    # ----------------------------------------------------------------------
    # Query & Stats Interface
    # ----------------------------------------------------------------------

    def get_recent_logs(self, limit: int = 10) -> List[Dict[str, Any]]:
        """Retrieve recent task log entries."""
        logs = self._read_json(self.task_logs_file, [])
        return logs[-limit:] if logs else []

    def get_insights(self) -> Dict[str, Any]:
        """Retrieve current self-learning insights."""
        return self._read_json(self.insights_file, {})

    def get_summary_stats(self) -> Dict[str, Any]:
        """High-level metrics for UI / diagnostics."""
        insights = self.get_insights()
        total = insights.get("total_tasks_logged", 0)
        successes = insights.get("success_count", 0)
        rate = round((successes / total * 100), 2) if total > 0 else 0.0

        return {
            "total_tasks": total,
            "successes": successes,
            "failures": insights.get("failure_count", 0),
            "success_rate_percent": rate,
            "learned_aliases_count": len(insights.get("learned_aliases", {})),
            "learned_aliases": insights.get("learned_aliases", {}),
            "recommendations": insights.get("recommendations", []),
        }
