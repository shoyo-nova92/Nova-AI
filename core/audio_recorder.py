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


    def record_command(self):

        print("[RECORDER] Waiting for speech...")

        audio_queue = queue.Queue()

        def callback(
            indata,
            frames,
            time_info,
            status
        ):

            audio_queue.put(
                indata[:,0].copy()
            )


        frames = []

        speech_started = False
        silence_time = None


        with sd.InputStream(
            samplerate=self.sample_rate,
            channels=1,
            dtype="float32",
            blocksize=1024,
            callback=callback
        ):

            while True:

                try:
                    chunk = audio_queue.get(timeout=5)

                except queue.Empty:
                    print("[RECORDER] Timeout")
                    return None

                rms = np.sqrt(
                    np.mean(
                        chunk ** 2
                    )
                )


                if rms > 0.015:

                    speech_started = True
                    silence_time = None

                    frames.append(chunk)

                    print(
                        "[RECORDER] speech"
                    )


                elif speech_started:

                    frames.append(chunk)

                    if silence_time is None:
                        silence_time=time.time()


                    if time.time()-silence_time > 1:

                        print(
                            "[RECORDER] finished"
                        )

                        break


        if not frames:
            return None


        return np.concatenate(frames)