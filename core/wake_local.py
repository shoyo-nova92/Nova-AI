import queue
import threading
import time

import numpy as np
import sounddevice as sd
import openwakeword
from openwakeword.model import Model


class LocalWake:
    """Real microphone wake-word detector backed by openWakeWord."""

    def __init__(
        self,
        wakeword_models=None,
        inference_framework="onnx",
        threshold=0.35,
    ):
        self.sample_rate = 16000
        self.frame_ms = 80
        self.frame_samples = 1280

        self.threshold = threshold
        self.wakeword_models = wakeword_models or ["hey_jarvis"]
        self.inference_framework = inference_framework

        self.q = queue.Queue()
        self.model = None

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

            print(
                f"[WAKE] Model loaded: "
                f"{self.wakeword_models}"
            )

            print(
                f"[WAKE] Threshold: "
                f"{self.threshold}"
            )

        except Exception as exc:
            print(f"[WAKE] Model initialization failed: {exc}")
            self.model = None

    def callback(self, indata, frames, time_info, status):
        if status:
            print(f"[WAKE AUDIO] {status}")

        if indata is not None:
            self.q.put(indata.copy())

    def listen_for_nova(self, timeout=0.5):
        """
        Listen to the microphone for one short interval.

        Returns:
            model label when detected
            "" otherwise
        """

        if self.model is None:
            time.sleep(min(0.1, timeout))
            return ""

        try:
            start_time = time.time()

            with sd.InputStream(
                samplerate=self.sample_rate,
                channels=1,
                dtype="int16",
                blocksize=self.frame_samples,
                callback=self.callback,
            ):

                while time.time() - start_time < timeout:

                    try:
                        data = self.q.get(timeout=0.1)

                    except queue.Empty:
                        continue

                    if data is None:
                        continue

                    # IMPORTANT:
                    # Keep the original int16 PCM format.
                    # This matches the diagnostic test.
                    audio = data[:, 0]

                    predictions = self.model.predict(audio)

                    if not isinstance(predictions, dict):
                        continue

                    for model_name, score in predictions.items():

                        if not isinstance(
                            score,
                            (int, float, np.floating)
                        ):
                            continue

                        score = float(score)

                        # Optional debug output
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

                            # Reset model state after detection.
                            self.model.reset()

                            return str(model_name).strip()

        except Exception as exc:
            print(
                f"\n[WAKE] Listener error: {exc}"
            )

            return ""

        return ""


class WakeKeyMonitor:
    """Keyboard activation key monitor."""

    def __init__(self, key_name="v"):

        self.key_name = key_name.lower()

        self._pressed = False
        self._was_pressed_flag = False

        self._lock = threading.Lock()

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
                f"press or hold '{self.key_name}'"
            )

        except Exception as exc:

            print(
                f"Keyboard wake-key listener unavailable: "
                f"{exc}"
            )

            self._listener = None

    def _normalize_key(self, key):

        try:

            if hasattr(key, "char") and key.char:
                return str(key.char).lower()

            if hasattr(key, "name"):
                return str(key.name).lower()

            return str(key).lower()

        except Exception:

            return str(key).lower()

    def _on_press(self, key):

        if self._listener is None:
            return

        key_name = self._normalize_key(key)

        if key_name == self.key_name:

            with self._lock:

                if not self._pressed:
                    self._was_pressed_flag = True

                self._pressed = True

            print(
                f"Wake activation key pressed: "
                f"'{self.key_name}'"
            )

    def _on_release(self, key):

        if self._listener is None:
            return

        key_name = self._normalize_key(key)

        if key_name == self.key_name:

            with self._lock:
                self._pressed = False

    def was_pressed(self):

        with self._lock:

            value = self._was_pressed_flag

            self._was_pressed_flag = False

            return value

    def is_held(self):

        with self._lock:
            return bool(self._pressed)

    def should_listen_for_wakeword(self):

        return (
            self.is_held()
            or self.was_pressed()
        )