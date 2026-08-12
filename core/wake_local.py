import queue
import threading
import time

import numpy as np
import sounddevice as sd

import openwakeword
from openwakeword.model import Model


class LocalWake:
    """A small openWakeWord-backed wake-word detector that listens on the mic.

    The default pretrained model is `hey_jarvis`. The user asked for "any pretrained for now";
    the engine is configured to accept a list of pretrained names, but defaults to the shipped
    model family that matches the package assets currently installed in this workspace.
    """

    def __init__(self, wakeword_models=None, inference_framework="onnx", threshold=0.65):
        self.sample_rate = 16000
        self.frame_ms = 80
        self.frame_samples = 1280  # 80 ms at 16kHz
        self.threshold = threshold

        self.wakeword_models = wakeword_models or ["hey_jarvis"]
        self.inference_framework = inference_framework

        self.q = queue.Queue()
        self.model = None

        self._initialize_wake_model()

    def _initialize_wake_model(self):
        try:
            openwakeword.utils.download_models(model_names=list(self.wakeword_models))
        except Exception as exc:
            print(f"openWakeWord model download warning: {exc}")

        try:
            self.model = Model(
                wakeword_models=list(self.wakeword_models),
                inference_framework=self.inference_framework,
            )
            print(f"wake word model loaded: {self.wakeword_models}")
        except Exception as exc:
            print(f"openWakeWord model init failed: {exc}")
            # keep the object usable for smoke tests but outside of real microphone detection
            self.model = None

    def callback(self, indata, frames, time_info, status):
        if indata is not None:
            self.q.put(indata.copy())

    def listen_for_nova(self, timeout=1.0):
        """Capture microphone audio and detect the selected wake phrase.

        Returns the model label when a score exceeds the configured threshold.
        Otherwise returns an empty string.
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

                    mono = data[:, 0].astype(np.float32) / 32768.0
                    mono = mono.reshape(-1)

                    predictions = self.model.predict(mono)

                    if not isinstance(predictions, dict):
                        continue

                    for model_name, score in predictions.items():
                        if isinstance(score, (int, float, np.floating)) and float(score) >= self.threshold:
                            label = str(model_name).strip()
                            print(f"[WAKE WORD DETECTED] {label} score={score:.4f}")
                            return label
                    time.sleep(0.01)
        except Exception as exc:
            print(f"Wake-word listener error: {exc}")
            return ""
        return ""


class WakeKeyMonitor:
    """Keyboard press and hold monitor for the activation key (default V)."""

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
            print(f"Wake activation key armed: press or hold '{self.key_name}'")
        except Exception as exc:
            print(f"Keyboard wake-key listener unavailable: {exc}")
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
            print(f"Wake activation key pressed: '{self.key_name}'")

    def _on_release(self, key):
        if self._listener is None:
            return
        key_name = self._normalize_key(key)
        if key_name == self.key_name:
            with self._lock:
                self._pressed = False

    def was_pressed(self):
        with self._lock:
            val = self._was_pressed_flag
            self._was_pressed_flag = False
            return val

    def is_held(self):
        with self._lock:
            return bool(self._pressed)

    def should_listen_for_wakeword(self):
        return self.is_held() or self.was_pressed()