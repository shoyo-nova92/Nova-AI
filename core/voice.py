import os
import tempfile

import numpy as np
import pyttsx3
import sounddevice as sd
import speech_recognition as sr
from faster_whisper import WhisperModel

from core.error_handler import NovaErrorHandler


class VoiceEngine:

    def __init__(self):
        self.sample_rate = 16000
        self.recognizer = sr.Recognizer()

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

    def listen(self):
        try:
            with sr.Microphone() as source:
                print("Listening for command...")
                self.recognizer.adjust_for_ambient_noise(source, duration=0.4)
                audio = self.recognizer.listen(source, timeout=5, phrase_time_limit=6)

            with tempfile.NamedTemporaryFile(delete=False, suffix=".wav") as temp_audio:
                temp_audio.write(audio.get_wav_data())
                temp_path = temp_audio.name

            segments, _ = self.model.transcribe(temp_path, beam_size=1)
            text = " ".join(segment.text for segment in segments).strip()
            os.remove(temp_path)

            print("You:", text)
            return text.lower()
        except Exception as e:
            NovaErrorHandler.handle(e, "VoiceEngine")
            return ""