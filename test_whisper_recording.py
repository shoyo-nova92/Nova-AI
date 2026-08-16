import time
import numpy as np
import sounddevice as sd
from faster_whisper import WhisperModel


SAMPLE_RATE = 16000
CHANNELS = 1
RECORD_SECONDS = 5


def main():

    print("=" * 60)
    print("Nova Whisper Recording Diagnostic")
    print("=" * 60)

    print("\nLoading Whisper Large-v3...")

    model = WhisperModel(
        "large-v3",
        device="cuda",
        compute_type="float16"
    )

    print("[OK] Whisper loaded.")

    print("\nRecording starts in 2 seconds...")
    time.sleep(2)

    print("\n>>> SPEAK NOW <<<")

    audio = sd.rec(
        int(RECORD_SECONDS * SAMPLE_RATE),
        samplerate=SAMPLE_RATE,
        channels=CHANNELS,
        dtype="float32",
    )

    sd.wait()

    print("Recording finished.")

    audio = audio[:, 0]

    print()
    print("Audio diagnostics:")
    print(f"Samples: {len(audio)}")
    print(f"Duration: {len(audio) / SAMPLE_RATE:.2f}s")
    print(f"Max amplitude: {np.max(np.abs(audio)):.5f}")
    print(f"RMS: {np.sqrt(np.mean(audio ** 2)):.5f}")

    print("\nTranscribing...")

    segments, info = model.transcribe(
        audio,
        beam_size=5,
        language="en",
        vad_filter=True,
        condition_on_previous_text=False,
    )

    text = " ".join(
        segment.text.strip()
        for segment in segments
    ).strip()

    print("\n" + "=" * 60)
    print("TRANSCRIPTION")
    print("=" * 60)

    print(text)

    print("\n" + "=" * 60)
    print("MODEL INFO")
    print("=" * 60)

    print("Language:", info.language)
    print("Language probability:", info.language_probability)


if __name__ == "__main__":
    main()