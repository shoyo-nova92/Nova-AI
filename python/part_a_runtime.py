import msvcrt
import time

from pathlib import Path

import numpy as np
import sounddevice as sd
import soundfile as sf

from openwakeword.model import Model
import os

BASE_DIR = os.path.dirname(
    os.path.dirname(
        os.path.abspath(__file__)
    )
)

CUDA_PATHS = [
    os.path.join(
        BASE_DIR,
        ".venv-win",
        "Lib",
        "site-packages",
        "nvidia",
        "cublas",
        "bin"
    ),
    os.path.join(
        BASE_DIR,
        ".venv-win",
        "Lib",
        "site-packages",
        "nvidia",
        "cudnn",
        "bin"
    ),
    os.path.join(
        BASE_DIR,
        ".venv-win",
        "Lib",
        "site-packages",
        "nvidia",
        "cuda_nvrtc",
        "bin"
    )
]

for path in CUDA_PATHS:
    if os.path.isdir(path):
        os.environ["PATH"] = (
            path
            + os.pathsep
            + os.environ.get("PATH", "")
        )

print("[CUDA] Runtime DLL paths configured.", flush=True)   
from faster_whisper import WhisperModel


# ============================================================
# CONFIGURATION
# ============================================================

SAMPLE_RATE = 16000
CHANNELS = 1

CHUNK_DURATION = 0.08
CHUNK_SIZE = int(
    SAMPLE_RATE * CHUNK_DURATION
)

WAKEWORD_THRESHOLD = 0.4

RMS_START_THRESHOLD = 0.02
RMS_END_THRESHOLD = 0.005

SPEECH_START_CONFIRMATION = 0.3
SPEECH_END_CONFIRMATION = 0.8

OUTPUT_DIRECTORY = Path("audio_output")


# ============================================================
# STATE
# ============================================================

STATE_IDLE = "IDLE"
STATE_LISTENING = "LISTENING"
STATE_TRANSCRIBING = "TRANSCRIBING"

state = STATE_IDLE


# ============================================================
# AUDIO HELPERS
# ============================================================

def calculate_rms(audio):
    return float(
        np.sqrt(
            np.mean(
                np.square(audio)
            )
        )
    )


# ============================================================
# LOAD MODELS
# ============================================================

print()
print("=" * 60)
print("NOVA PART A RUNTIME")
print("=" * 60)
print()

print("[INIT] Loading OpenWakeWord...")

wakeword_model = Model(
    wakeword_models=["hey_jarvis_v0.1"],
    inference_framework="onnx"
)

print("[INIT] OpenWakeWord: READY")

print("[INIT] Loading Whisper Large-v3...")

stt_model = WhisperModel(
    "large-v3",
    device="cuda",
    compute_type="float16"
)

print("[INIT] Whisper Large-v3: READY")

OUTPUT_DIRECTORY.mkdir(
    parents=True,
    exist_ok=True
)

print()
print("[INIT] All Part A models ready.")
print()
print("Press V to bypass wake word.")
print("Say 'Hey Jarvis' to activate.")
print("Press Q to quit.")
print()


# ============================================================
# RUNTIME
# ============================================================

with sd.InputStream(
    samplerate=SAMPLE_RATE,
    channels=CHANNELS,
    dtype="float32",
    blocksize=CHUNK_SIZE
) as stream:

    while True:

        # ----------------------------------------------------
        # GLOBAL KEYBOARD CONTROLS
        # ----------------------------------------------------

        if msvcrt.kbhit():

            key = msvcrt.getwch().lower()

            if key == "q":

                print()
                print("[EXIT] Nova stopped.")
                break

            if key == "v" and state == STATE_IDLE:

                print()
                print(
                    "[WAKE] V-key bypass activated."
                )

                state = STATE_LISTENING

                wakeword_model.reset()


        # ----------------------------------------------------
        # IDLE / WAKE WORD
        # ----------------------------------------------------

        if state == STATE_IDLE:

            audio, overflowed = stream.read(
                CHUNK_SIZE
            )

            audio_int16 = (
                (audio[:, 0] * 32767)
                .astype(np.int16)
            )

            prediction = wakeword_model.predict(
                audio_int16
            )

            score = float(
                prediction.get(
                    "hey_jarvis_v0.1",
                    0.0
                )
            )

            if score >= WAKEWORD_THRESHOLD:

                print()
                print(
                    f"[WAKE] Hey Jarvis "
                    f"(score={score:.4f})"
                )

                wakeword_model.reset()

                state = STATE_LISTENING

                continue

        # ----------------------------------------------------
        # LISTENING
        # ----------------------------------------------------

        if state == STATE_LISTENING:

            print(
                "[LISTENING] Speak now..."
            )

            audio_buffer = []

            speech_candidate_start = None
            silence_candidate_start = None

            keyboard_override = False

            while True:

                # --------------------------------------------
                # KEYBOARD OVERRIDE
                # --------------------------------------------

                if msvcrt.kbhit():

                    key = msvcrt.getwch().lower()

                    if key == "v":

                        print()
                        print(
                            "[KEYBOARD] V-key override activated.",
                            flush=True
                        )

                        keyboard_override = True

                        break

                    if key == "q":

                        print()
                        print(
                            "[EXIT] Nova stopped.",
                            flush=True
                        )

                        raise SystemExit


                # --------------------------------------------
                # AUDIO
                # --------------------------------------------

                audio, overflowed = stream.read(
                    CHUNK_SIZE
                )

                audio = (
                    audio
                    .copy()
                    .reshape(-1)
                )

                audio_buffer.append(
                    audio
                )

                rms = calculate_rms(
                    audio
                )

                now = time.monotonic()


                # --------------------------------------------
                # SPEECH START
                # --------------------------------------------

                if speech_candidate_start is None:

                    if (
                        rms
                        >= RMS_START_THRESHOLD
                    ):

                        speech_candidate_start = now

                    continue


                # --------------------------------------------
                # SPEECH ACTIVE
                # --------------------------------------------

                if rms > RMS_END_THRESHOLD:

                    silence_candidate_start = None

                    continue


                # --------------------------------------------
                # POSSIBLE END OF UTTERANCE
                # --------------------------------------------

                if silence_candidate_start is None:

                    silence_candidate_start = now

                elif (
                    now
                    - silence_candidate_start
                    >= SPEECH_END_CONFIRMATION
                ):

                    break


            # ------------------------------------------------
            # KEYBOARD OVERRIDE
            # ------------------------------------------------

            if keyboard_override:

                print(
                    "[KEYBOARD_REQUEST]",
                    flush=True
                )

                state = STATE_IDLE

                wakeword_model.reset()

                break


            # ------------------------------------------------
            # BUILD AUDIO
            # ------------------------------------------------

            audio_data = np.concatenate(
                audio_buffer
            )

            timestamp = int(
                time.time()
            )

            audio_path = (
                OUTPUT_DIRECTORY
                / f"runtime_{timestamp}.wav"
            )

            sf.write(
                audio_path,
                audio_data,
                SAMPLE_RATE
            )

            print(
                f"[AUDIO] Saved: "
                f"{audio_path}"
            )

            state = STATE_TRANSCRIBING


        # ----------------------------------------------------
        # TRANSCRIPTION
        # ----------------------------------------------------

        if state == STATE_TRANSCRIBING:

            print(
                "[STT] Transcribing "
                "with Whisper Large-v3..."
            )

            segments, info = (
                stt_model.transcribe(
                    str(audio_path)
                )
            )

            text = " ".join(
                segment.text.strip()
                for segment in segments
            ).strip()

            print()
            print(
                f"[TEXT_READY] {text}",
                flush=True
            )

            state = STATE_IDLE

            wakeword_model.reset()

            print(
                "[STATE] Returning to IDLE.",
                flush=True
            )
            print(
                "[PART_A] Text handoff complete.",
                flush=True
            )

            break
            print()