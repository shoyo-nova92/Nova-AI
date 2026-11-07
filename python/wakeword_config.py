"""Shared local Hey Nova model and calibrated detection settings."""
import json
from pathlib import Path

MODEL_DIRECTORY = Path(__file__).resolve().parent.parent / 'models' / 'wakeword'
settings = json.loads((MODEL_DIRECTORY / 'hey_nova.json').read_text(encoding='utf-8'))
WAKEWORD_MODEL_PATH = MODEL_DIRECTORY / settings['model']
WAKEWORD_MODEL_KEY = settings['key']
WAKEWORD_PHRASE = settings['phrase']
WAKEWORD_THRESHOLD = float(settings['threshold'])

if not WAKEWORD_MODEL_PATH.is_file():
    raise FileNotFoundError(f'Hey Nova wakeword model is missing: {WAKEWORD_MODEL_PATH}')
if WAKEWORD_MODEL_KEY != WAKEWORD_MODEL_PATH.stem:
    raise ValueError('Wakeword prediction key must match the ONNX filename stem')
if not 0 < WAKEWORD_THRESHOLD < 1:
    raise ValueError('Wakeword threshold must be between zero and one')
