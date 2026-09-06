import queue
import time
import numpy as np
import sounddevice as sd


class AudioRecorder:

    def __init__(
        self,
        sample_rate=16000
    ):
        self.sample_rate = sample_rate


    def record_command(self, timeout=None, interrupt_check=None, min_speech_chunks=2, speech_threshold=0.020, max_duration=15.0):
        print("[RECORDER] Waiting for speech...")

        audio_queue = queue.Queue()

        def callback(indata, frames, time_info, status):
            audio_queue.put(indata[:, 0].copy())

        frames = []
        pre_buffer = []
        speech_started = False
        consecutive_speech = 0
        silence_time = None
        recording_start_time = None
        start_time = time.time()

        with sd.InputStream(
            samplerate=self.sample_rate,
            channels=1,
            dtype="float32",
            blocksize=1024,
            callback=callback
        ):
            while True:
                if interrupt_check and interrupt_check():
                    return None

                try:
                    chunk = audio_queue.get(timeout=0.1)
                except queue.Empty:
                    if timeout is not None and not speech_started and (time.time() - start_time > timeout):
                        return None
                    continue

                rms = float(np.sqrt(np.mean(chunk ** 2)))

                if not speech_started:
                    # Maintain last 3 chunks (~190ms) to capture speech onset cleanly
                    pre_buffer.append(chunk)
                    if len(pre_buffer) > 3:
                        pre_buffer.pop(0)

                    if rms >= speech_threshold:
                        consecutive_speech += 1
                        if consecutive_speech >= min_speech_chunks:
                            speech_started = True
                            recording_start_time = time.time()
                            silence_time = None
                            frames.extend(pre_buffer)
                            frames.append(chunk)
                            print("[RECORDER] Speech started...")
                    else:
                        consecutive_speech = 0

                    if timeout is not None and (time.time() - start_time > timeout):
                        return None

                else:
                    frames.append(chunk)

                    # Cap maximum recording length to prevent runaways on background noise
                    if recording_start_time and (time.time() - recording_start_time > max_duration):
                        print("[RECORDER] Max speech duration reached")
                        break

                    if rms < 0.015:
                        if silence_time is None:
                            silence_time = time.time()
                        elif time.time() - silence_time > 1.0:
                            print("[RECORDER] Finished speech capture.")
                            break
                    else:
                        silence_time = None

        if not frames:
            return None

        return np.concatenate(frames)