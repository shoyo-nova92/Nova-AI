# Hey Nova wakeword

- Wake phrase: **Hey Nova**, including the requested **hey-No-Va** pronunciation.
- Model: `hey_nova.onnx`; custom openWakeWord classifier, not renamed Jarvis weights.
- Settings: `hey_nova.json`; shared by the runtime and microphone tester.
- Training and feature test results: `training_report.json`.
- End-to-end 80 ms streaming calibration/test results: `streaming_validation.json`.

The selected threshold is **0.95**. Streaming testing detected **359/364**
held-out positive examples (98.6%) and rejected **9/9** Hey Jarvis examples.
It falsely accepted **8/273** synthetic hard negatives, including some
examples of Hey Noah, Hey Nola, Hey November, and Hey no. A separate
continuous-background test recorded **5** false activation events in
**5.35 hours**. These measurements are baseline results, not guarantees.
The final integration check loaded the model from outside the project
directory and processed 25 frames from the default microphone successfully.

From the Nova project directory, run:

```powershell
.\.venv-win\Scripts\python.exe python\wakeword_test.py
```

Say “hey-No-Va” and watch the score and wake detection messages. Use normal
speech, quiet speech, and your usual microphone distance. The included
evaluation uses held-out synthetic voices and public background audio;
performance on your own voice and room is not yet measured. False
activations remain possible.

The model runs locally through Nova's existing ONNX/openWakeWord pipeline.
It does not invoke speech transcription while waiting for the wake phrase.
Training scripts, dataset attribution, dependency versions, and reproduction
instructions are in `tools/wakeword-training/README.md`.
