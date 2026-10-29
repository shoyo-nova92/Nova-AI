# Hey Nova training

The exported model lives in `models/wakeword/hey_nova.onnx`. Nova and the
microphone tester read its companion `hey_nova.json` through
`python/wakeword_config.py`. Paths resolve from the source location, so the
application does not depend on its current working directory.

## Training method

This trains a new small DNN on openWakeWord's existing 96-dimensional speech
embeddings. It does not rename or fine-tune the Jarvis detector. The DNN has
two 64-unit layers with layer normalization and ReLU, followed by a sigmoid.
Its input is 16 embedding frames. Runtime audio remains mono 16 kHz PCM,
processed in 1,280-sample (80 ms) blocks.

Piper's English LibriTTS-R medium model generates speech from 904 speakers.
Speakers are divided into disjoint training, calibration, and test groups.
Positive examples include standard `Hey Nova!` and explicit IPA
`hˈeɪ nˈoʊvɑː` for the requested `hey-No-Va` pronunciation. Training includes
20,000 augmented positive examples and 10,000 augmented hard negatives.
These are augmentations of a smaller set of generated utterances, not
20,000 independently recorded people. Augmentation varies rate/pitch, gain,
noise, short room reflections, and microphone bandwidth.

98,304 precomputed negative examples are sampled in 32 blocks spread across
the published 2,000-hour ACAV100M feature corpus (about 35 hours sampled).
The separate published 10.7-hour continuous evaluation set is split in half
for threshold calibration and final evaluation. Synthetic hard negatives
include similar words, ordinary commands, Nova alone, and Hey Jarvis.

The exported ONNX is then calibrated and evaluated again using
`Model.predict` on 80 ms audio frames, with separate held-out speakers.
`training_report.json` and `streaming_validation.json` record measurements.
They describe synthetic/evaluation-set performance; they do not establish
recall on the owner's voice, microphone, accent, or room.

## Run again (PowerShell from the project root)

```powershell
.\.venv-win\Scripts\python.exe -m venv --system-site-packages tools\wakeword-training\venv
.\tools\wakeword-training\venv\Scripts\python.exe -m pip install -r tools\wakeword-training\requirements.txt
.\tools\wakeword-training\venv\Scripts\python.exe -u tools\wakeword-training\train_hey_nova.py
.\.venv-win\Scripts\python.exe -u tools\wakeword-training\validate_streaming.py
.\.venv-win\Scripts\python.exe python\wakeword_test.py
```

The training script reuses completed caches. Keep `data/speaker_split.json`
with the generated speech. To change phrases or synthesis settings, use a
fresh data directory to avoid reusing stale examples. Downloads use public
assets; no account or paid service is required. The training script uses the
feature extractor models already installed in Nova's `.venv-win`.

## Sources and attribution

- [openWakeWord](https://github.com/dscripka/openWakeWord): Apache-2.0 code;
  consult upstream for the pretrained embedding model's terms.
- [Piper LibriTTS-R model card](https://huggingface.co/rhasspy/piper-voices/blob/main/en/en_US/libritts_r/medium/MODEL_CARD):
  model derived from LibriTTS-R, dataset CC BY 4.0. Piper synthesis package
  version and dependencies are pinned in `requirements.txt`.
- [David Scripka's openWakeWord features](https://huggingface.co/datasets/davidscripka/openwakeword_features):
  ACAV100M negative features and separate DiPCo/Santa Barbara/MUSDB evaluation
  features; the dataset declares CC BY-NC-SA 4.0. Preserve those terms when
  distributing or reusing the training artifacts.

Training data, the separate environment, and the reference upstream checkout
are cached under this directory and ignored by Git. The ONNX, settings,
reports, and training scripts are retained as project artifacts.
