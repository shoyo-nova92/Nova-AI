import json
from unittest.mock import MagicMock

from core.step_router import StepRouter
from core.task_translator import TaskTranslator
from core.visual_executor import VisualExecutor
from core.execution_policy import ExecutionPolicy


class MockPerceptionService:
    def __init__(self, screen_states=None):
        self.screen_states = list(screen_states or [{"active_window": "VSCode", "visible_text": ["Unsaved *"]}])
        self.call_count = 0

    def capture(self):
        idx = min(self.call_count, len(self.screen_states) - 1)
        self.call_count += 1
        return self.screen_states[idx]

    def build_context_summary(self, vision_data):
        vision_data = vision_data or {}
        window = vision_data.get("active_window", "unknown")
        texts = " ".join(vision_data.get("visible_text", []))
        return f"{window}: {texts}"


class MockLLMClient:
    def __init__(self, response_json):
        self.response_json = response_json

    def generate(self, prompt="", system_prompt=None):
        return json.dumps(self.response_json)


def test_step_router():
    router = StepRouter()
    # Fast paths
    assert router.route_step({"type": "application", "action": "open_app"}) == "fast"
    assert router.route_step({"type": "browser", "action": "search"}) == "fast"
    assert router.route_step({"type": "filesystem", "action": "create_file"}) == "fast"
    assert router.route_step({"type": "terminal", "action": "run_pytest"}) == "fast"
    assert router.route_step({"type": "git", "action": "git_commit"}) == "fast"
    assert router.route_step({"type": "terminal", "action": "git_status"}) == "fast"

    # Visual paths
    assert router.route_step({"type": "gui", "action": "visual_interaction"}) == "visual"
    assert router.route_step({"type": "unknown", "action": "something"}) == "visual"
    assert router.route_step("plain string") == "visual"


def test_task_translator_visual_action():
    translator = TaskTranslator()
    action = translator.translate("click save button")
    assert action is not None
    assert action["type"] == "gui"
    assert action["action"] == "visual_interaction"
    assert action["target"] == "click save button"

    key_action = translator.translate("press ctrl+s")
    assert key_action is not None
    assert key_action["type"] == "gui"
    assert key_action["action"] == "visual_interaction"
    assert key_action["target"] == "press ctrl+s"


def test_visual_executor_rejection():
    perception = MockPerceptionService()
    llm = MockLLMClient({"action": "key", "target": "ctrl+s", "expected_text": "Saved"})
    executor = VisualExecutor(
        perception_service=perception,
        llm_client=llm,
        approval_callback=lambda action: False,  # User rejects
    )

    result = executor.execute_step({"type": "gui", "action": "visual_interaction", "target": "press ctrl+s"})
    assert result["success"] is False
    assert "not approved" in result["reason"]


def test_visual_executor_success_with_screen_change():
    # Screen changes from Unsaved to Saved
    states = [
        {"active_window": "VSCode", "visible_text": ["Unsaved *"], "ui_elements": []},
        {"active_window": "VSCode", "visible_text": ["Saved successfully"], "ui_elements": []},
    ]
    perception = MockPerceptionService(screen_states=states)
    llm = MockLLMClient({"action": "key", "target": "ctrl+s", "expected_text": "Saved successfully"})
    mock_ui = MagicMock()
    mock_ui.click_element.return_value = True

    executor = VisualExecutor(
        perception_service=perception,
        llm_client=llm,
        ui_executor=mock_ui,
        approval_callback=lambda action: True,  # User approves
    )
    # Mock pyautogui hotkey so it doesn't send real keypresses
    executor._execute_gui_action = MagicMock(return_value=True)

    result = executor.execute_step({"type": "gui", "action": "visual_interaction", "target": "save file"})
    assert result["success"] is True
    assert result["executed_via"] == "visual"
    assert "verified screen change" in result["reason"]


def test_visual_executor_max_attempts_cap():
    # Screen NEVER changes
    states = [
        {"active_window": "VSCode", "visible_text": ["Unsaved *"], "ui_elements": []},
        {"active_window": "VSCode", "visible_text": ["Unsaved *"], "ui_elements": []},
        {"active_window": "VSCode", "visible_text": ["Unsaved *"], "ui_elements": []},
        {"active_window": "VSCode", "visible_text": ["Unsaved *"], "ui_elements": []},
    ]
    perception = MockPerceptionService(screen_states=states)
    llm = MockLLMClient({"action": "key", "target": "ctrl+s", "expected_text": "Done"})
    mock_ui = MagicMock()

    executor = VisualExecutor(
        perception_service=perception,
        llm_client=llm,
        ui_executor=mock_ui,
        approval_callback=lambda action: True,
    )
    executor._execute_gui_action = MagicMock(return_value=True)

    result = executor.execute_step({"type": "gui", "action": "visual_interaction", "target": "save file"})
    assert result["success"] is False
    assert result["attempts"] == 3
    assert "did not produce a verified screen change" in result["reason"]


def test_execution_policy_gui_actions():
    policy = ExecutionPolicy()
    click_classification = policy.classify({"type": "gui", "action": "click"})
    assert click_classification["status"] == ExecutionPolicy.CONFIRMATION_REQUIRED
    assert click_classification["allowed"] is False

    key_classification = policy.classify({"type": "gui", "action": "key"})
    assert key_classification["status"] == ExecutionPolicy.CONFIRMATION_REQUIRED
    assert key_classification["allowed"] is False


if __name__ == "__main__":
    test_step_router()
    test_task_translator_visual_action()
    test_visual_executor_rejection()
    test_visual_executor_success_with_screen_change()
    test_visual_executor_max_attempts_cap()
    test_execution_policy_gui_actions()
    print("ALL VISUAL PIPELINE TESTS PASSED")
