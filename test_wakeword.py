import openwakeword
from openwakeword.model import Model

openwakeword.utils.download_models(model_names=["hey_jarvis"])

model = Model(wakeword_models=["hey_jarvis"], inference_framework="onnx")
print("wake word model loaded:", list(model.models.keys()))
print("wake word detection test: openWakeWord package is connected")