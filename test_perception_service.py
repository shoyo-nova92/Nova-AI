import time
from core.perception_service import PerceptionService


class DummyVisionEngine:
    def __init__(self, should_fail=False):
        self.should_fail = should_fail
        self.captured = False

    def analyze_screen(self):
        self.captured = True
        if self.should_fail:
            raise RuntimeError("Capture failed")
        return {
            "active_window": "Visual Studio Code - main.py",
            "visible_text": ["File", "Save Failed", "Permission Denied", "Line 42"],
            "ui_elements": [{"text": "Retry", "x": 100, "y": 200}],
        }


def test_perception_should_perceive_keywords():
    service = PerceptionService()
    assert service.should_perceive("I cannot save this file") is True
    assert service.should_perceive("Check the screen and help") is True
    assert service.should_perceive("Why am I getting this error?") is True
    assert service.should_perceive("What is quantum computing?") is False


def test_perception_should_perceive_recent_window_change():
    service = PerceptionService(active_window_change_window_seconds=5.0)
    now = 1000.0
    # Changed 2 seconds ago -> within 5s window
    assert service.should_perceive("What should I do?", last_active_window_change_ts=998.0, now=now) is True
    # Changed 10 seconds ago -> outside 5s window
    assert service.should_perceive("What should I do?", last_active_window_change_ts=990.0, now=now) is False


def test_perception_ui_callbacks_lifecycle():
    events = []

    def mock_hide():
        events.append("hide")

    def mock_show():
        events.append("show")

    vision = DummyVisionEngine()
    service = PerceptionService(
        vision_engine=vision,
        hide_ui_callback=mock_hide,
        show_ui_callback=mock_show,
        capture_delay_seconds=0.0,
    )

    data = service.capture()
    assert vision.captured is True
    assert events == ["hide", "show"], f"Events were: {events}"
    assert data["active_window"] == "Visual Studio Code - main.py"


def test_perception_ui_callbacks_on_failure():
    events = []

    def mock_hide():
        events.append("hide")

    def mock_show():
        events.append("show")

    vision = DummyVisionEngine(should_fail=True)
    service = PerceptionService(
        vision_engine=vision,
        hide_ui_callback=mock_hide,
        show_ui_callback=mock_show,
        capture_delay_seconds=0.0,
    )

    try:
        service.capture()
        assert False, "Should have raised exception"
    except RuntimeError:
        pass

    assert events == ["hide", "show"], f"Show was not called on failure: {events}"


def test_perception_context_summary():
    service = PerceptionService()
    summary = service.build_context_summary({
        "active_window": "Editor",
        "visible_text": ["Save error", "disk full"],
    })
    assert "Active window: Editor" in summary
    assert "Save error | disk full" in summary

    empty_summary = service.build_context_summary({})
    assert "unknown" in empty_summary.lower()


if __name__ == "__main__":
    test_perception_should_perceive_keywords()
    test_perception_should_perceive_recent_window_change()
    test_perception_ui_callbacks_lifecycle()
    test_perception_ui_callbacks_on_failure()
    test_perception_context_summary()
    print("ALL PERCEPTION SERVICE TESTS PASSED")
