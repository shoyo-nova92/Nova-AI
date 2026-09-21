from pathlib import Path
import wave

import numpy as np
import torch
from silero_vad import load_silero_vad, get_speech_timestamps


AUDIO_DIRECTORY = Path("audio_output")

print("Loading Silero VAD...")

model = load_silero_vad()
model.eval()

print("Silero VAD loaded successfully.")
print()

audio_files = sorted(
    AUDIO_DIRECTORY.glob("utterance_*.wav")
)

if not audio_files:
    print("No WAV files found.")
    raise SystemExit(1)


for audio_path in audio_files:

    print("=" * 60)
    print(f"File: {audio_path}")

    with wave.open(
        str(audio_path),
        "rb"
    ) as wav_file:

        sample_rate = wav_file.getframerate()
        frames = wav_file.readframes(
            wav_file.getnframes()
        )

    audio = np.frombuffer(
        frames,
        dtype=np.int16
    ).astype(np.float32)

    audio /= 32768.0

    audio_tensor = torch.from_numpy(
        audio
    )

    duration = len(audio) / sample_rate

    print(f"Sample rate: {sample_rate}")
    print(f"Duration: {duration:.2f}s")
    print("Running Silero VAD...")

    speech_timestamps = get_speech_timestamps(
        audio_tensor,
        model,
        sampling_rate=sample_rate
    )

    if not speech_timestamps:

        print("Speech detected: NO")
        print("Result: NO SPEECH")

        continue

    total_speech = sum(
        timestamp["end"] - timestamp["start"]
        for timestamp in speech_timestamps
    )

    total_speech_seconds = (
        total_speech / sample_rate
    )

    print("Speech detected: YES")
    print(
        f"Speech segments: "
        f"{len(speech_timestamps)}"
    )
    print(
        f"Total speech: "
        f"{total_speech_seconds:.2f}s"
    )

    for index, timestamp in enumerate(
        speech_timestamps,
        start=1
    ):

        start = timestamp["start"] / sample_rate
        end = timestamp["end"] / sample_rate

        print(
            f"  Segment {index}: "
            f"{start:.2f}s → {end:.2f}s"
        )


print()
print("Silero VAD test complete.")