import sounddevice as sd
import numpy as np
import time

print("Recording microphone for 10 seconds...")
print("DO NOT SPEAK.")

for i in range(10):
    audio = sd.rec(
        16000,
        samplerate=16000,
        channels=1,
        dtype="int16"
    )

    sd.wait()

    audio = audio.reshape(-1)

    rms = np.sqrt(np.mean(audio.astype(np.float32) ** 2))
    peak = np.max(np.abs(audio))

    print(
        f"{i + 1:02d}s | RMS={rms:.2f} | PEAK={peak}"
    )
