"""Calibrate and test exported ONNX through the actual 80ms runtime API."""
import json
from pathlib import Path
import time
import numpy as np
import soundfile as sf
from openwakeword.model import Model
from train_hey_nova import augmented

ROOT = Path(__file__).resolve().parent
OUT = ROOT.parent.parent / 'models' / 'wakeword'
report = json.loads((OUT / 'training_report.json').read_text())
model = Model(wakeword_models=[str(OUT / 'hey_nova.onnx')], inference_framework='onnx')
assert list(model.models) == ['hey_nova'], list(model.models)

def evaluate(split):
    records = []
    paths = sorted((ROOT / 'data' / 'speech' / split).glob('*.wav'))
    started = time.monotonic()
    for i, path in enumerate(paths):
        audio = sf.read(path, dtype='int16')[0]
        variants = [('clean', audio)]
        if path.name.startswith('positive'):
            variants.append(('augmented', augmented(audio.astype(np.float32)/32768)))
        for variant, x in variants:
            # One second of lead-in plus a full second after the utterance:
            # score every 80ms with the same inference path as the microphone.
            x = np.pad(x, (16000, 16000))
            x = np.pad(x, (0, (-len(x)) % 1280))
            model.reset()
            scores = [float(model.predict(x[j:j+1280])['hey_nova']) for j in range(0, len(x), 1280)]
            records.append({'file':path.name, 'variant':variant, 'positive':path.name.startswith('positive'), 'max_score':max(scores)})
        if (i+1) % 100 == 0:
            print(f'Streaming {split} {i+1}/{len(paths)} elapsed {time.monotonic()-started:.0f}s', flush=True)
    return records

val = evaluate('val')
def stats(records, threshold):
    positives = [r for r in records if r['positive']]
    negatives = [r for r in records if not r['positive']]
    clean = [r for r in positives if r['variant']=='clean']
    aug = [r for r in positives if r['variant']=='augmented']
    return {'positive_examples':len(positives), 'positive_recall':sum(r['max_score']>=threshold for r in positives)/len(positives),
            'clean_recall':sum(r['max_score']>=threshold for r in clean)/len(clean),
            'augmented_recall':sum(r['max_score']>=threshold for r in aug)/len(aug),
            'hard_negative_examples':len(negatives), 'hard_negative_false_activations':sum(r['max_score']>=threshold for r in negatives),
            'hard_negative_false_rate':sum(r['max_score']>=threshold for r in negatives)/len(negatives)}

base = report['threshold']
candidates = []
for threshold in np.linspace(base, .98, 100):
    result = stats(val, float(threshold))
    metric = result['positive_recall'] - 2*max(0, result['hard_negative_false_rate']-.01)
    candidates.append((metric, -threshold, result))
_, negative_threshold, calibration = max(candidates, key=lambda x:(x[0],x[1]))
threshold = float(-negative_threshold)
print('Streaming calibration', threshold, calibration, flush=True)
test = evaluate('test')
result = {'threshold':threshold, 'calibration':calibration, 'test':stats(test, threshold),
          'model_key':list(model.models)[0], 'chunk_samples':1280, 'sample_rate':16000,
          'calibration_records':val, 'test_records':test}
(OUT / 'streaming_validation.json').write_text(json.dumps(result, indent=2))
(OUT / 'hey_nova.json').write_text(json.dumps({'model':'hey_nova.onnx', 'key':'hey_nova', 'phrase':'Hey Nova', 'threshold':threshold}, indent=2))
print('STREAMING COMPLETE', json.dumps({k:v for k,v in result.items() if not k.endswith('records')}), flush=True)
