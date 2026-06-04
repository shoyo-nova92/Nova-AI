import sounddevice as sd
import numpy as np
from faster_whisper import WhisperModel
import pyttsx3
from core.error_handler import NovaErrorHandler
import speech_recognition as sr

class VoiceEngine:

    def __init__(self):
        self.sample_rate = 16000

        self.model = WhisperModel(
            "large-v3",
            device="cuda",
            compute_type="float16"
        )

        self.tts = pyttsx3.init()
        self.tts.setProperty("rate", 180)

    def speak(self, text):
        print("Nova:", text)
        self.tts.say(text)
        self.tts.runAndWait()

    def listen(self):
        with sr.Microphone() as source:
            print("Listening for command...")

            self.recognizer.adjust_for_ambient_noise(
                source,
                duration=0.5
            )

            # stop listening faster after silence
            audio = self.recognizer.listen(
                source,
                timeout=5,
                phrase_time_limit=6
            )

        try:
            with tempfile.NamedTemporaryFile(
                delete=False,
                suffix=".wav"
            ) as temp_audio:
                temp_audio.write(audio.get_wav_data())
                temp_path = temp_audio.name

            segments, _ = self.model.transcribe(
                temp_path,
                beam_size=1
            )

            text = " ".join(
                segment.text
                for segment in segments
            ).strip()

            os.remove(temp_path)

            print("You:", text)

            return text.lower()

        except Exception as e:
            NovaErrorHandler.handle(
                e,
                "VoiceEngine"
            )
            return ""