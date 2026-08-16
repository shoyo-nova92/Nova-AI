import os
import tempfile

import numpy as np
import pyttsx3
import sounddevice as sd
from faster_whisper import WhisperModel

from core.error_handler import NovaErrorHandler


class VoiceEngine:

    def __init__(self):
        self.sample_rate = 16000

        try:
            self.model = WhisperModel(
                "large-v3",
                device="cuda",
                compute_type="float16"
            )
        except Exception:
            self.model = WhisperModel(
                "large-v3",
                device="cpu",
                compute_type="int8"
            )

        try:
            self.tts = pyttsx3.init()
            self.tts.setProperty("rate", 180)
        except Exception:
            self.tts = None

    def speak(self, text):
        print("Nova:", text)
        if self.tts is not None:
            try:
                self.tts.say(text)
                self.tts.runAndWait()
            except Exception:
                return

    def listen(self, duration=5):
        temp_path = None

        try:
            print("Listening for command...")

            # Record microphone audio directly using sounddevice.
            audio = sd.rec(
                int(duration * self.sample_rate),
                samplerate=self.sample_rate,
                channels=1,
                dtype="float32",
            )

            sd.wait()

            # Convert stereo/2D array into a mono 1D array.
            audio = audio.flatten()

            if audio.size == 0:
                return ""

            # Save audio as a temporary WAV file.
            import scipy.io.wavfile as wavfile

            with tempfile.NamedTemporaryFile(
                delete=False,
                suffix=".wav"
            ) as temp_audio:

                temp_path = temp_audio.name

            wavfile.write(
                temp_path,
                self.sample_rate,
                audio,
            )

            # Transcribe using Whisper Large-v3.
            segments, _ = self.model.transcribe(
                temp_path,
                beam_size=1,
            )

            text = " ".join(
                segment.text
                for segment in segments
            ).strip()

            print("You:", text)

            return text.lower()

        except Exception as exc:
            NovaErrorHandler.handle(exc, "VoiceEngine")
            return ""

        finally:
            if temp_path and os.path.exists(temp_path):
                try:
                    os.remove(temp_path)
                except Exception:
                    pass