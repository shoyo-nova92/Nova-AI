import queue
import numpy as np
import sounddevice as sd
import webrtcvad
from faster_whisper import WhisperModel


class LocalWake:

    def __init__(self):
        self.sample_rate = 16000
        self.frame_ms = 30
        self.frame_samples = 480   # 30ms @ 16kHz

        self.q = queue.Queue()

        self.vad = webrtcvad.Vad(2)

        self.model = WhisperModel(
            "large-v3",
            device="cuda",
            compute_type="float16"
        )

    def callback(self, indata, frames, time, status):
        self.q.put(indata.copy())

    def listen_for_nova(self):
        print("Waiting for wake word: Nova")

        speech_chunks = []
        speaking = False
        silence_frames = 0

        with sd.InputStream(
            samplerate=self.sample_rate,
            channels=1,
            dtype="int16",
            blocksize=self.frame_samples,
            callback=self.callback
        ):

            while True:
                data = self.q.get()
                frame = data.flatten().tobytes()

                is_speech = self.vad.is_speech(frame, self.sample_rate)

                if is_speech:
                    speaking = True
                    speech_chunks.append(frame)
                    silence_frames = 0

                elif speaking:
                    speech_chunks.append(frame)
                    silence_frames += 1

                if speaking and silence_frames > 10:

                    audio_bytes = b"".join(speech_chunks)

                    audio_np = np.frombuffer(
                        audio_bytes,
                        dtype=np.int16
                    ).astype(np.float32) / 32768.0

                    segments, _ = self.model.transcribe(
                        audio_np,
                        language="en"
                    )

                    text = " ".join(seg.text for seg in segments).lower().strip()

                    if text:
                        print("Heard:", text)

                    if text:
                        if "exit" in text or "quit" in text:
                            return text

                    if "nova" in text:
                        print("Wake word detected")
                        return text

                    speech_chunks = []
                    speaking = False
                    silence_frames = 0