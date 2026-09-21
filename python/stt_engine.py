from faster_whisper import WhisperModel


class STTEngine:
    def __init__(self):
        self.model = WhisperModel(
            "small",
            device="cuda",
            compute_type="float16"
        )

    def transcribe(self, audio_path):
        segments, info = self.model.transcribe(audio_path)
    
        text = " ".join(
            segment.text.strip()
            for segment in segments
        )

        return text.strip()