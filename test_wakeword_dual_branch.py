import unittest
from unittest.mock import MagicMock, patch
import queue

from core.wake_local import LocalWake, WakeKeyMonitor
from nova import UIEventQueue, InputNormalizer, SessionManager


class TestWakewordDualBranch(unittest.TestCase):
    def test_input_normalizer(self):
        self.assertEqual(InputNormalizer.normalize("  Hey   Nova  "), "hey nova")
        self.assertEqual(InputNormalizer.normalize(""), "")

    def test_ui_event_queue(self):
        ui_q = UIEventQueue()
        ui_q.show_ui()
        ui_q.set_state("Listening", (0, 220, 120))
        
        item1 = ui_q.q.get_nowait()
        item2 = ui_q.q.get_nowait()
        
        self.assertEqual(item1, ("show", None))
        self.assertEqual(item2, ("set_state", ("Listening", (0, 220, 120))))

    @patch("pynput.keyboard.Listener")
    def test_wake_key_monitor_press(self, mock_listener):
        monitor = WakeKeyMonitor(key_name="v")
        self.assertFalse(monitor.was_pressed())
        
        mock_key = MagicMock()
        mock_key.char = "v"
        monitor._on_press(mock_key)
        
        self.assertTrue(monitor.was_pressed())
        self.assertFalse(monitor.was_pressed())  # Reset after check

    @patch("openwakeword.model.Model")
    @patch("openwakeword.utils.download_models")
    def test_local_wake_initialization(self, mock_download, mock_model):
        wake = LocalWake(wakeword_models=["hey_jarvis"])
        mock_download.assert_called_once_with(model_names=["hey_jarvis"])
        self.assertIsNotNone(wake.model)


if __name__ == "__main__":
    unittest.main()
