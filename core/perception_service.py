"""Decision and capture layer for screen-aware Nova requests."""

import time

from core.vision_engine import VisionEngine


class PerceptionService:
    """Adds cheap perception gating around the existing VisionEngine.

    UI callbacks are optional so headless runtime tests remain independent of
    PyQt.  In the desktop app they are supplied by Nova's UIEventQueue.
    """

    KEYWORDS = ("this", "here", "cannot", "can't", "error", "stuck", "save", "screen", "doing")

    def __init__(
        self,
        vision_engine=None,
        hide_ui_callback=None,
        show_ui_callback=None,
        capture_delay_seconds=0.2,
        active_window_change_window_seconds=5.0,
    ):
        self.vision = vision_engine or VisionEngine()
        self.hide_ui_callback = hide_ui_callback or (lambda: None)
        self.show_ui_callback = show_ui_callback or (lambda: None)
        self.capture_delay_seconds = capture_delay_seconds
        self.active_window_change_window_seconds = active_window_change_window_seconds

    def set_ui_callbacks(self, hide_ui_callback=None, show_ui_callback=None):
        self.hide_ui_callback = hide_ui_callback or (lambda: None)
        self.show_ui_callback = show_ui_callback or (lambda: None)

    def should_perceive(self, command, last_active_window_change_ts=None, now=None):
        """Pure keyword/time-window decision with no capture side effects."""
        normalized = (command or "").lower()
        keyword_match = any(keyword in normalized for keyword in self.KEYWORDS)

        if now is None:
            now = time.time()
        recent_window_change = (
            last_active_window_change_ts is not None
            and 0 <= now - last_active_window_change_ts <= self.active_window_change_window_seconds
        )
        return keyword_match or recent_window_change

    def capture(self):
        """Capture through VisionEngine while keeping Nova's Orb out of frame."""
        hidden = False
        try:
            self.hide_ui_callback()
            hidden = True
            if self.capture_delay_seconds:
                time.sleep(self.capture_delay_seconds)
            return self.vision.analyze_screen()
        finally:
            if hidden:
                self.show_ui_callback()

    def build_context_summary(self, vision_data):
        """Produce compact LLM-ready active-window and OCR context."""
        vision_data = vision_data or {}
        active_window = vision_data.get("active_window") or "unknown"
        if isinstance(active_window, dict):
            active_window = active_window.get("title") or active_window.get("name") or "unknown"

        visible_text = vision_data.get("visible_text") or []
        if isinstance(visible_text, dict):
            visible_text = visible_text.get("text") or visible_text.get("elements") or []
        if isinstance(visible_text, str):
            lines = [visible_text]
        else:
            lines = []
            for item in visible_text:
                if isinstance(item, dict):
                    text = item.get("text") or item.get("label")
                else:
                    text = item
                if text:
                    lines.append(str(text))

        ocr_excerpt = " | ".join(lines[:10]).strip()
        if ocr_excerpt:
            return f"Active window: {active_window}. Visible text: {ocr_excerpt}."
        return f"Active window: {active_window}. No readable on-screen text was detected."
