import queue
import time

import numpy as np
import sounddevice as sd
import openwakeword
from openwakeword.model import Model


SAMPLE_RATE = 16000
FRAME_MS = 80
FRAME_SAMPLES = 1280

WAKEWORD = "hey_jarvis"

# Start with the documented/default-ish threshold.
# We are NOT using this to decide detection yet;
# we mainly want to see the raw scores.
THRESHOLD = 0.30


audio_queue = queue.Queue()


def audio_callback(indata, frames, time_info, status):
    if status:
        print(f"[AUDIO STATUS] {status}")

    audio_queue.put(indata.copy())


def main():
    print("=" * 60)
    print("Nova Wake Word Diagnostic")
    print("=" * 60)

    print(f"Wake word model : {WAKEWORD}")
    print(f"Sample rate     : {SAMPLE_RATE}")
    print(f"Frame size      : {FRAME_SAMPLES}")
    print(f"Threshold       : {THRESHOLD}")
    print()

    # Make sure the pretrained model exists.
    try:
        print("[1] Loading wake-word model...")

        openwakeword.utils.download_models(
            model_names=[WAKEWORD]
        )

        model = Model(
            wakeword_models=[WAKEWORD],
            inference_framework="onnx",
        )

        print("[OK] Model loaded.")
        print()

    except Exception as exc:
        print("[ERROR] Could not load wake-word model.")
        print(exc)
        return

    print("[2] Opening microphone...")
    print()

    try:
        with sd.InputStream(
            samplerate=SAMPLE_RATE,
            channels=1,
            dtype="int16",
            blocksize=FRAME_SAMPLES,
            callback=audio_callback,
        ):

            print("[OK] Microphone stream started.")
            print()
            print("=" * 60)
            print("SAY:  HEY JARVIS")
            print("=" * 60)
            print()
            print("Raw model scores will appear below.")
            print("Press CTRL+C to stop.")
            print()

            last_print = 0

            while True:

                try:
                    data = audio_queue.get(timeout=1.0)
                except queue.Empty:
                    continue

                if data is None:
                    continue

                # Keep the original 16-bit PCM data.
                # This follows the format used by the openWakeWord
                # microphone examples.
                audio = data[:, 0]

                predictions = model.predict(audio)

                if not isinstance(predictions, dict):
                    print("[WARNING] Model returned:", predictions)
                    continue

                score = predictions.get(WAKEWORD)

                if score is None:
                    print(
                        "[WARNING] Model did not return "
                        f"'{WAKEWORD}'"
                    )
                    print("Predictions:", predictions)
                    continue

                score = float(score)

                # Avoid absolutely flooding the terminal.
                now = time.time()

                if now - last_print >= 0.10:
                    print(
                        f"\r[WAKE DEBUG] "
                        f"{WAKEWORD}: {score:.4f}",
                        end="",
                        flush=True,
                    )
                    last_print = now

                if score >= THRESHOLD:
                    print()
                    print()
                    print("=" * 60)
                    print(
                        f"[WAKE WORD DETECTED] "
                        f"{WAKEWORD} "
                        f"score={score:.4f}"
                    )
                    print("=" * 60)
                    print()

                    # Reset the model state after detection.
                    model.reset()

    except KeyboardInterrupt:
        print()
        print()
        print("[STOP] Diagnostic stopped by user.")

    except Exception as exc:
        print()
        print()
        print("[ERROR] Microphone/wake-word loop failed:")
        print(exc)


if __name__ == "__main__":
    main()