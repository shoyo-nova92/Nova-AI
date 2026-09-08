"""Conservative, approval-gated execution for GUI-only planned steps."""

import time

from core.execution_policy import ExecutionPolicy
from core.llm_json import extract_json_object
from core.perception_service import PerceptionService
from core.ui_executor import UIExecutor
from core.ui_semantics_engine import UISemanticsEngine


class VisualExecutor:
    MAX_ATTEMPTS = 3

    def __init__(
        self,
        perception_service=None,
        llm_client=None,
        ui_executor=None,
        ui_semantics=None,
        policy=None,
        approval_callback=None,
    ):
        self.perception = perception_service or PerceptionService()
        self.llm_client = llm_client
        self.ui_executor = ui_executor or UIExecutor()
        self.ui_semantics = ui_semantics or UISemanticsEngine()
        self.policy = policy or ExecutionPolicy()
        self.approval_callback = approval_callback or self._request_approval

    @staticmethod
    def _request_approval(action):
        try:
            answer = input(
                "Nova wants to perform GUI action "
                f"{action.get('action')} ({action.get('target')}). Approve? (yes/no): "
            )
        except (EOFError, KeyboardInterrupt):
            return False
        return answer.strip().lower() in {"y", "yes"}

    def _text_signature(self, vision_data):
        return self.perception.build_context_summary(vision_data).lower()

    def _choose_gui_action(self, step, semantic_elements):
        if self.llm_client is None:
            return None

        prompt = (
            "Choose exactly one safe GUI interaction for this planned step.\n"
            f"Step: {step}\n"
            f"Semantic elements: {semantic_elements}\n\n"
            "Return JSON only: "
            '{"action":"click"|"key", "target":"visible OCR text or ctrl+s", '
            '"expected_text":"text expected after the action"}. '
            "Use click only for a listed visible element."
        )
        parsed = extract_json_object(self.llm_client.generate(prompt=prompt))
        if not parsed:
            return None

        action_name = str(parsed.get("action") or "").lower()
        target = parsed.get("target")
        if action_name not in {"click", "key"} or not isinstance(target, str) or not target.strip():
            return None
        return {
            "type": "gui",
            "action": action_name,
            "action_type": action_name,
            "target": target.strip(),
            "expected_text": str(parsed.get("expected_text") or "").strip(),
        }

    def _matches_expected_text(self, vision_data, expected_text):
        if not expected_text:
            return False
        return expected_text.lower() in self.perception.build_context_summary(vision_data).lower()

    def _execute_gui_action(self, gui_action, ocr_elements):
        if gui_action["action"] == "click":
            return bool(self.ui_executor.click_element(ocr_elements, gui_action["target"]))
        try:
            import pyautogui
            keys = [key.strip() for key in gui_action["target"].split("+") if key.strip()]
            if not keys:
                return False
            pyautogui.hotkey(*keys)
            return True
        except Exception:
            return False

    def execute_step(self, step, max_attempts=None):
        """Run a bounded, approval-gated visual attempt and verify screen change."""
        attempts_limit = min(self.MAX_ATTEMPTS, max_attempts or self.MAX_ATTEMPTS)
        if self.llm_client is None:
            return {"success": False, "reason": "visual execution has no LLM client", "attempts": 0}

        for attempt in range(1, attempts_limit + 1):
            before = self.perception.capture()
            ocr_elements = before.get("ui_elements") or []
            semantic_elements = self.ui_semantics.analyze(ocr_elements)
            gui_action = self._choose_gui_action(step, semantic_elements)
            if not gui_action:
                return {"success": False, "reason": "LLM did not provide a valid GUI action", "attempts": attempt}

            policy_result = self.policy.classify(gui_action)
            if not policy_result.get("allowed") and not self.approval_callback(gui_action):
                return {"success": False, "reason": "GUI action was not approved", "attempts": attempt, "action": gui_action}

            if not self._execute_gui_action(gui_action, ocr_elements):
                continue

            time.sleep(0.15)
            after = self.perception.capture()
            changed = self._text_signature(before) != self._text_signature(after)
            expected_seen = self._matches_expected_text(after, gui_action["expected_text"])
            if changed or expected_seen:
                return {
                    "success": True,
                    "reason": "visual action produced a verified screen change",
                    "attempts": attempt,
                    "action": gui_action,
                    "executed_via": "visual",
                }

        return {
            "success": False,
            "reason": "visual action did not produce a verified screen change",
            "attempts": attempts_limit,
            "executed_via": "visual",
        }
