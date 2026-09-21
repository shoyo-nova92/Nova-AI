from pathlib import Path
import time

from faster_whisper import WhisperModel


AUDIO_DIRECTORY = Path("audio_output")

MODEL_NAME = "large-v3"
DEVICE = "cuda"
COMPUTE_TYPE = "float16"


print("Loading Faster-Whisper...")
print(f"Model: {MODEL_NAME}")
print(f"Device: {DEVICE}")
print(f"Compute type: {COMPUTE_TYPE}")
print()

load_start = time.perf_counter()

model = WhisperModel(
    MODEL_NAME,
    device=DEVICE,
    compute_type=COMPUTE_TYPE
)

load_time = time.perf_counter() - load_start

print(
    f"Model loaded successfully in "
    f"{load_time:.2f}s"
)
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
    print("Transcribing...")

    start_time = time.perf_counter()

    segments, info = model.transcribe(
        str(audio_path)
    )

    text = " ".join(
        segment.text.strip()
        for segment in segments
    ).strip()

    elapsed = (
        time.perf_counter()
        - start_time
    )

    print()
    print(f"Detected language: {info.language}")
    print(
        f"Language probability: "
        f"{info.language_probability:.3f}"
    )
    print(f"Transcription: {text}")
    print(
        f"Transcription time: "
        f"{elapsed:.2f}s"
    )


print()
print("Whisper Large-v3 test complete.")