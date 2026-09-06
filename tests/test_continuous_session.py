import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import queue
import time
from unittest.mock import MagicMock
from nova import InputNormalizer, handle_runtime_result, UIEventQueue

def test_input_normalizer():
    assert InputNormalizer.normalize("hey jarvis open chrome") == "open chrome"
    assert InputNormalizer.normalize("jarvis open notepad") == "open notepad"
    assert InputNormalizer.normalize("open chrome open chrome") == "open chrome"
    assert InputNormalizer.normalize("hey nova what time is it") == "what time is it"
    print("[PASS] test_input_normalizer")

def test_handle_runtime_result_exit():
    ui_queue = UIEventQueue()
    spine = MagicMock()
    result = {
        "status": "handled",
        "category": "exit",
        "received": {"command": "bye"},
        "response": "Goodbye!",
    }
    is_exit = handle_runtime_result(ui_queue, spine, result)
    assert is_exit is True
    
    # Verify UI event queue received the Bye state
    action, data = ui_queue.q.get_nowait() # Observing
    assert action == "set_state"
    action, data = ui_queue.q.get_nowait() # Bye
    assert action == "set_state"
    assert data[0] == "Bye"
    print("[PASS] test_handle_runtime_result_exit")

def test_handle_runtime_result_conversational():
    ui_queue = UIEventQueue()
    spine = MagicMock()
    result = {
        "status": "handled",
        "category": "conversational",
        "received": {"command": "how are you"},
        "response": "I am feeling great!",
        "state": "COMPLETED"
    }
    voice_mock = MagicMock()
    is_exit = handle_runtime_result(ui_queue, spine, result, voice_engine=voice_mock)
    assert is_exit is False
    voice_mock.speak.assert_called_once_with("I am feeling great!")
    
    states = []
    while not ui_queue.q.empty():
        action, data = ui_queue.q.get_nowait()
        if action == "set_state":
            states.append(data[0])
    
    assert "Thinking" in states
    print("[PASS] test_handle_runtime_result_conversational")

def test_ui_queue_methods():
    ui_queue = UIEventQueue()
    ui_queue.show_ui()
    ui_queue.hide_ui()
    ui_queue.set_state("Listening", (0, 220, 120))
    ui_queue.set_text("test command")
    ui_queue.quit_app()

    actions = [ui_queue.q.get_nowait()[0] for _ in range(5)]
    assert actions == ["show", "hide", "set_state", "set_text", "quit"]
    print("[PASS] test_ui_queue_methods")

if __name__ == "__main__":
    test_input_normalizer()
    test_handle_runtime_result_exit()
    test_handle_runtime_result_conversational()
    test_ui_queue_methods()
    print("\nALL CONTINUOUS SESSION TESTS PASSED!")
