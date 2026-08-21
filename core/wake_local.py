import queue
import threading
import time

import numpy as np
import sounddevice as sd
import openwakeword
from openwakeword.model import Model


class LocalWake:
    """Continuous microphone wake-word detector backed by openWakeWord."""

    def __init__(
        self,
        wakeword_models=None,
        inference_framework="onnx",
        threshold=0.35,
    ):
        self.sample_rate = 16000
        self.frame_samples = 1280  # 80 ms @ 16 kHz
        self.threshold = threshold

        self.wakeword_models = wakeword_models or ["hey_jarvis"]
        self.inference_framework = inference_framework

        self.q = queue.Queue(maxsize=20)

        self.model = None
        self.stream = None

        self._running = False
        self._lock = threading.Lock()

        self._initialize_wake_model()

    def _initialize_wake_model(self):
        try:
            openwakeword.utils.download_models(
                model_names=list(self.wakeword_models)
            )
        except Exception as exc:
            print(f"[WAKE] Model download warning: {exc}")

        try:
            self.model = Model(
                wakeword_models=list(self.wakeword_models),
                inference_framework=self.inference_framework,
            )

            print(f"[WAKE] Model loaded: {self.wakeword_models}")
            print(f"[WAKE] Threshold: {self.threshold}")

        except Exception as exc:
            print(f"[WAKE] Model initialization failed: {exc}")
            self.model = None

    def _callback(self, indata, frames, time_info, status):
        if status:
            print(f"[WAKE AUDIO] {status}")

        if indata is None:
            return

        audio = indata[:, 0].copy()

        try:
            self.q.put_nowait(audio)
        except queue.Full:
            # Drop the oldest frame rather than allowing latency to build.
            try:
                self.q.get_nowait()
            except queue.Empty:
                pass

            try:
                self.q.put_nowait(audio)
            except queue.Full:
                pass

    def start(self):
        """Start one persistent microphone stream."""

        if self.model is None:
            print("[WAKE] Cannot start: model unavailable.")
            return False

        with self._lock:

            if self._running:
                return True

            # Clear any stale audio.
            while True:
                try:
                    self.q.get_nowait()
                except queue.Empty:
                    break

            try:
                self.stream = sd.InputStream(
                    samplerate=self.sample_rate,
                    channels=1,
                    dtype="int16",
                    blocksize=self.frame_samples,
                    callback=self._callback,
                )

                self.stream.start()
                self._running = True

                print("[WAKE] Continuous microphone stream started.")

                return True

            except Exception as exc:
                print(f"[WAKE] Microphone stream failed: {exc}")
                self.stream = None
                self._running = False
                return False

    def stop(self):
        """Stop the persistent microphone stream."""

        with self._lock:

            self._running = False

            if self.stream is not None:
                try:
                    self.stream.stop()
                except Exception:
                    pass

                try:
                    self.stream.close()
                except Exception:
                    pass

                self.stream = None

        print("[WAKE] Microphone stream stopped.")

    def listen_for_nova(self, timeout=None):
        """
        Continuously consume microphone frames and detect the wake word.

        timeout=None means keep listening indefinitely.
        """

        if self.model is None:
            return ""

        if not self._running:
            if not self.start():
                return ""

        start_time = time.monotonic()

        while self._running:

            if timeout is not None:
                if time.monotonic() - start_time >= timeout:
                    return ""

            try:
                audio = self.q.get(timeout=0.25)

            except queue.Empty:
                continue

            if audio is None:
                continue

            # Ensure exact int16 mono PCM.
            audio = np.asarray(audio, dtype=np.int16).reshape(-1)

            if audio.size == 0:
                continue

            # Feed the exact 80 ms frame to openWakeWord.
            predictions = self.model.predict(audio)

            if not isinstance(predictions, dict):
                continue

            for model_name, score in predictions.items():

                try:
                    score = float(score)
                except (TypeError, ValueError):
                    continue

                print(
                    f"\r[WAKE DEBUG] "
                    f"{model_name}: {score:.4f}",
                    end="",
                    flush=True,
                )

                if score >= self.threshold:

                    print()

                    print(
                        f"[WAKE WORD DETECTED] "
                        f"{model_name} "
                        f"score={score:.4f}"
                    )

                    # Prevent immediate retriggering from residual audio.
                    self.model.reset()

                    # Clear frames already queued before detection.
                    while True:
                        try:
                            self.q.get_nowait()
                        except queue.Empty:
                            break

                    return str(model_name).strip()

        return ""


class WakeKeyMonitor:
    """Keyboard activation key monitor using a one-shot press event."""

    def __init__(self, key_name="v"):
        self.key_name = key_name.lower()

        self._press_event = threading.Event()
        self._lock = threading.Lock()
        self._pressed = False

        try:
            from pynput import keyboard

            self._keyboard = keyboard

            self._listener = keyboard.Listener(
                on_press=self._on_press,
                on_release=self._on_release,
            )

            self._listener.start()

            print(
                f"Wake activation key armed: "
                f"press '{self.key_name}'"
            )

        except Exception as exc:
            print(
                f"Keyboard wake-key listener unavailable: {exc}"
            )

            self._listener = None

    def _normalize_key(self, key):
        try:
            if hasattr(key, "char") and key.char:
                return str(key.char).lower()

            if hasattr(key, "name") and key.name:
                return str(key.name).lower()

            return str(key).lower()

        except Exception:
            return str(key).lower()

    def _on_press(self, key):
        if self._listener is None:
            return

        key_name = self._normalize_key(key)

        if key_name != self.key_name:
            return

        with self._lock:
            if self._pressed:
                return

            self._pressed = True
            self._press_event.set()

    def _on_release(self, key):
        if self._listener is None:
            return

        key_name = self._normalize_key(key)

        if key_name != self.key_name:
            return

        with self._lock:
            self._pressed = False

    def was_pressed(self):
        """
        Consume exactly one V press.

        Returns True once for each physical press.
        Holding V does not generate repeated activations.
        """
        if self._press_event.is_set():
            self._press_event.clear()
            return True

        return False

    def is_held(self):
        """
        Kept for compatibility with older code.
        Not used for activation.
        """
        with self._lock:
            return self._pressed

    def should_listen_for_wakeword(self):
        """
        Compatibility helper.
        """
        return self.was_pressed()