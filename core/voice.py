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

            audio = sd.rec(
                int(duration * self.sample_rate),
                samplerate=self.sample_rate,
                channels=1,
                dtype="float32",
            )

            sd.wait()

            audio = audio.flatten()

            if audio.size == 0:
                return ""

            rms = float(
                np.sqrt(
                    np.mean(
                        np.square(audio.astype(np.float32))
                    )
                )
            )

            peak = float(
                np.max(
                    np.abs(audio)
                )
            )

            print(
                f"[VOICE] Audio level: "
                f"RMS={rms:.5f} "
                f"PEAK={peak:.5f}"
            )

            if rms < 0.01 and peak < 0.05:
                print("[VOICE] No meaningful audio detected.")
                return ""

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

            segments, info = self.model.transcribe(
                temp_path,
                beam_size=1,
                vad_filter=True,
            )

            segments = list(segments)

            if not segments:
                print("[VOICE] Whisper detected no speech.")
                return ""

            accepted_segments = []

            for segment in segments:

                text = segment.text.strip()

                if not text:
                    continue

                print(
                    "[VOICE] Segment:",
                    repr(text),
                    "| no_speech_prob=",
                    f"{segment.no_speech_prob:.3f}"
                )

                if segment.no_speech_prob > 0.60:
                    print(
                        "[VOICE] Ignoring low-confidence "
                        "speech segment."
                    )
                    continue

                accepted_segments.append(text)

            text = " ".join(
                accepted_segments
            ).strip()

            if not text:
                print("[VOICE] No reliable speech detected.")
                return ""

            print("You:", text)

            return text.lower()

        except Exception as exc:

            NovaErrorHandler.handle(
                exc,
                "VoiceEngine"
            )

            return ""

        finally:

            if temp_path and os.path.exists(temp_path):

                try:
                    os.remove(temp_path)

                except Exception:
                    pass