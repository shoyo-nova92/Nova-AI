from faster_whisper import WhisperModel

print("Loading Whisper Large-v3...")

model = WhisperModel(
    "large-v3",
    device="cuda",
    compute_type="float16",
)

print("Whisper Large-v3 loaded successfully on CUDA.")