import torch
import numpy as np
from silero_vad import load_silero_vad, get_speech_timestamps


class VADEngine:
    def __init__(self):
        self.model = load_silero_vad()

        self.model.eval()

    def detect(self, audio, sample_rate=16000):
        if not isinstance(audio, torch.Tensor):
            audio = torch.from_numpy(
                np.asarray(audio, dtype=np.float32)
            )

        if audio.ndim > 1:
            audio = audio.mean(dim=1)

        speech_timestamps = get_speech_timestamps(
            audio,
            self.model,
            sampling_rate=sample_rate
        )

        return speech_timestamps