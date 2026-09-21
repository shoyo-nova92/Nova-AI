import time
import wave
from collections import deque
from pathlib import Path

import numpy as np
import sounddevice as sd


SAMPLE_RATE = 16000
CHANNELS = 1

CHUNK_DURATION = 0.1
CHUNK_SIZE = int(SAMPLE_RATE * CHUNK_DURATION)

RMS_START_THRESHOLD = 0.02
RMS_END_THRESHOLD = 0.005

AVERAGE_WINDOW = 5

SPEECH_START_CONFIRMATION = 0.3
SPEECH_END_CONFIRMATION = 0.8

OUTPUT_DIRECTORY = Path("audio_output")


def calculate_rms(audio):
    return float(
        np.sqrt(
            np.mean(
                np.square(audio)
            )
        )
    )


def save_wav(audio_chunks, output_path):
    audio = np.concatenate(audio_chunks)

    audio = np.clip(audio, -1.0, 1.0)

    pcm_audio = (
        audio * 32767
    ).astype(np.int16)

    with wave.open(
        str(output_path),
        "wb"
    ) as wav_file:

        wav_file.setnchannels(CHANNELS)
        wav_file.setsampwidth(2)
        wav_file.setframerate(SAMPLE_RATE)

        wav_file.writeframes(
            pcm_audio.tobytes()
        )


print("Starting Nova audio buffer...")
print(f"Sample rate: {SAMPLE_RATE}")
print(f"Chunk size: {CHUNK_SIZE}")
print()
print("Speak normally.")
print("Each detected utterance will be saved.")
print("Press Ctrl+C to stop.")
print()

OUTPUT_DIRECTORY.mkdir(
    parents=True,
    exist_ok=True
)

rms_history = deque(
    maxlen=AVERAGE_WINDOW
)

speech_active = False

speech_candidate_start = None
silence_candidate_start = None

audio_buffer = []

utterance_counter = 0


with sd.InputStream(
    samplerate=SAMPLE_RATE,
    channels=CHANNELS,
    dtype="float32",
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

        rms = calculate_rms(audio)

        rms_history.append(rms)

        average_rms = (
            sum(rms_history)
            / len(rms_history)
        )

        now = time.monotonic()

        if not speech_active:

            if average_rms >= RMS_START_THRESHOLD:

                if speech_candidate_start is None:

                    speech_candidate_start = now

                elif (
                    now - speech_candidate_start
                    >= SPEECH_START_CONFIRMATION
                ):

                    speech_active = True

                    speech_candidate_start = None
                    silence_candidate_start = None

                    audio_buffer = []

                    print(
                        "\n[SPEECH START]"
                    )
                    print(
                        "[BUFFER] Recording utterance..."
                    )

            else:

                speech_candidate_start = None

        else:

            audio_buffer.append(audio)

            if average_rms <= RMS_END_THRESHOLD:

                if silence_candidate_start is None:

                    silence_candidate_start = now

                elif (
                    now - silence_candidate_start
                    >= SPEECH_END_CONFIRMATION
                ):

                    speech_active = False

                    silence_candidate_start = None
                    speech_candidate_start = None

                    utterance_counter += 1

                    output_path = (
                        OUTPUT_DIRECTORY
                        / f"utterance_{utterance_counter:03d}.wav"
                    )

                    save_wav(
                        audio_buffer,
                        output_path
                    )

                    duration = (
                        sum(
                            len(chunk)
                            for chunk in audio_buffer
                        )
                        / SAMPLE_RATE
                    )

                    print(
                        "[SPEECH END]"
                    )

                    print(
                        f"[BUFFER] Saved: {output_path}"
                    )

                    print(
                        f"[BUFFER] Duration: "
                        f"{duration:.2f}s"
                    )

                    audio_buffer = []

            else:

                silence_candidate_start = None