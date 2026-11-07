# Hey Nova wakeword

- Wake phrase: **Hey Nova**.
- Model: `hey_nova.onnx`; custom openWakeWord classifier.
- Settings: `hey_nova.json`; shared by the runtime and microphone tester.

The selected threshold is **0.95**. Training used synthesized speech with
disjoint speaker groups, augmented positive and hard-negative examples, and
published negative audio features. Evaluation on synthetic voices and
background audio does not establish accuracy for your voice, microphone,
accent, or room. Similar phrases and background audio can cause false
activations.

After completing the [voice setup](../../README.md#optional-voice-input),
run from the Nova project directory:

```powershell
.\build\Nova.exe
```

Say **Hey Nova**, wait for the listening prompt, then speak your request.

The model runs locally through Nova's existing ONNX/openWakeWord pipeline.
It does not invoke speech transcription while waiting for the wake phrase.

## Sources and attribution

- [openWakeWord](https://github.com/dscripka/openWakeWord): Apache-2.0 code;
  consult upstream for the pretrained embedding model's terms.
- [Piper LibriTTS-R model card](https://huggingface.co/rhasspy/piper-voices/blob/main/en/en_US/libritts_r/medium/MODEL_CARD):
  synthesized training speech used this model, derived from LibriTTS-R
  (dataset CC BY 4.0).
- [David Scripka's openWakeWord features](https://huggingface.co/datasets/davidscripka/openwakeword_features):
  ACAV100M negative features and DiPCo/Santa Barbara/MUSDB evaluation features.
  The dataset declares CC BY-NC-SA 4.0. Consult these terms before
  redistribution or commercial use of training-derived assets.

Training data and training tools are unnecessary for running the included
classifier and are not part of the runtime checkout.
