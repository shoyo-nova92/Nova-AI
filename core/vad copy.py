import torch
from silero_vad import load_silero_vad, get_speech_timestamps


class VoiceActivityDetector:

    def __init__(self):

        self.model = load_silero_vad()

    def detect(
        self,
        audio,
        sample_rate=16000
    ):

        tensor = torch.tensor(
            audio,
            dtype=torch.float32
        )

        return get_speech_timestamps(
            tensor,
            self.model,
            sampling_rate=sample_rate
        )

    def contains_speech(
        self,
        audio,
        sample_rate=16000
    ):

        return len(
            self.detect(
                audio,
                sample_rate
            )
        ) > 0

        tensor = torch.tensor(
            audio,
            dtype=torch.float32
        )

        timestamps = self.get_speech_timestamps(
            tensor,
            self.model,
            sampling_rate=sample_rate
        )

        return timestamps

        return len(
            self.detect(
                audio,
                sample_rate
            )
        ) > 0