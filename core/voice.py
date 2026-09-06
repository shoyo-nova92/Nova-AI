import os
import tempfile

import numpy as np
import pyttsx3
import scipy.io.wavfile as wavfile

from faster_whisper import WhisperModel

from core.error_handler import NovaErrorHandler
from core.audio_recorder import AudioRecorder

class VoiceEngine:

    def __init__(self):

        self.sample_rate = 16000

        self.recorder = AudioRecorder(
        self.sample_rate
        )

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

    def listen(self, duration=5, timeout=None, interrupt_check=None):
        print("[VOICE ENGINE] LISTEN STARTED")
        temp_path = None

        try:
            print("Listening for command...")

            audio = self.recorder.record_command(timeout=timeout, interrupt_check=interrupt_check)

            if audio is None:
                return ""

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
                beam_size=5,
                vad_filter=True,
                condition_on_previous_text=False,
                temperature=0.0,
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
                    f"{segment.no_speech_prob:.3f}",
                    "| avg_logprob=",
                    f"{segment.avg_logprob:.3f}"
                )


                if segment.no_speech_prob > 0.60:

                    print(
                        "[VOICE] Rejected: high no_speech probability"
                    )

                    continue


                if segment.avg_logprob < -1.0:

                    print(
                        "[VOICE] Rejected: low transcription confidence"
                    )

                    continue

                if segment.compression_ratio > 2.4:
                    print(
                        "[VOICE] Rejected: repetition hallucination"
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