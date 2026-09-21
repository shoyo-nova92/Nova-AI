import time

import numpy as np
import sounddevice as sd
from openwakeword.model import Model


SAMPLE_RATE = 16000
CHANNELS = 1

CHUNK_DURATION = 0.08
CHUNK_SIZE = int(
    SAMPLE_RATE * CHUNK_DURATION
)

WAKEWORD_THRESHOLD = 0.4


print("Loading OpenWakeWord...")

model = Model(
    wakeword_models=["hey_jarvis_v0.1"],
    inference_framework="onnx"
)

print("OpenWakeWord loaded successfully.")
print(f"Wake word: Hey Jarvis")
print(f"Threshold: {WAKEWORD_THRESHOLD}")
print()
print("Test:")
print("1. Stay silent for a few seconds.")
print("2. Speak normal sentences without saying the wake word.")
print("3. Say 'Hey Jarvis' quietly.")
print("4. Repeat several times.")
print()
print("Press Ctrl+C to stop.")
print()


last_print = 0.0
max_score = 0.0

with sd.InputStream(
    samplerate=SAMPLE_RATE,
    channels=CHANNELS,
    dtype="int16",
    blocksize=CHUNK_SIZE
) as stream:

    while True:

        audio, overflowed = stream.read(
            CHUNK_SIZE
        )

        audio = (
            audio
            .copy()
            .reshape(-1)
        )

        prediction = model.predict(
            audio
        )

        score = float(
            prediction.get(
                "hey_jarvis_v0.1",
                0.0
            )
        )

        max_score = max(
            max_score,
            score
        )

        now = time.monotonic()

        if now - last_print >= 1.0:

            print(
                f"Current={score:.4f} | "
                f"Max={max_score:.4f}"
            )

            last_print = now

        if score >= WAKEWORD_THRESHOLD:

            print()
            print("========================================")
            print("[WAKE DETECTED] Hey Jarvis")
            print(f"Confidence: {score:.4f}")
            print(f"Max score: {max_score:.4f}")
            print("========================================")
            print()

            model.reset()

            max_score = 0.0