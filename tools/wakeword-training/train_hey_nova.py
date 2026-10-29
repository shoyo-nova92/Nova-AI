"""Reproducible Windows training for a custom openWakeWord ONNX classifier.

Uses Piper LibriTTS speakers, augmented speech, and openWakeWord's published
ACAV100M negative embeddings. All data and intermediate artifacts stay local.
Run with tools/wakeword-training/venv/Scripts/python.exe -u this_file.py.
"""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import time
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='backslashreplace', line_buffering=True)
sys.stderr.reconfigure(encoding='utf-8', errors='backslashreplace', line_buffering=True)

os.environ.setdefault('OMP_NUM_THREADS', '4')
import numpy as np
import requests
import scipy.signal as sig
import soundfile as sf

ROOT = Path(__file__).resolve().parent
DATA = ROOT / 'data'
DATA.mkdir(exist_ok=True)
RNG = np.random.default_rng(20261028)

def download(url, path):
    if path.exists():
        return
    print('Downloading', path.name, flush=True)
    for attempt in range(4):
        try:
            with requests.get(url, stream=True, timeout=(30, 120)) as r:
                r.raise_for_status()
                with path.with_suffix(path.suffix + '.part').open('wb') as f:
                    for chunk in r.iter_content(1024 * 1024):
                        f.write(chunk)
            path.with_suffix(path.suffix + '.part').replace(path)
            return
        except Exception:
            if attempt == 3:
                raise
            time.sleep(2)

def prepare():
    voice = 'en_US-libritts_r-medium.onnx'
    base = 'https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/libritts_r/medium/'
    hf = 'https://huggingface.co/datasets/davidscripka/openwakeword_features/resolve/main/'
    with concurrent.futures.ThreadPoolExecutor(3) as pool:
        list(pool.map(lambda args: download(*args), [
            (base + voice, DATA / voice),
            (base + voice + '.json', DATA / (voice + '.json')),
            (hf + 'validation_set_features.npy', DATA / 'validation_set_features.npy')]))
    if not (DATA / 'negative_features.npy').exists():
        # Distribute 98,304 samples across the 2,000-hour corpus, rather than
        # taking the beginning of one recording. Preserve the original float16.
        url = hf + 'openwakeword_features_ACAV100M_2000_hrs_16bit.npy'
        def fetch(i):
            path = DATA / f'negative_part_{i:02}.npy'
            if path.exists():
                return np.load(path)
            row = int(i * (5625000 - 3072) / 31)
            lo, hi = 128 + row * 3072, 128 + (row + 3072) * 3072 - 1
            for attempt in range(4):
                try:
                    r = requests.get(url + f'?nova_chunk={i}', headers={'Range': f'bytes={lo}-{hi}'}, timeout=120)
                    r.raise_for_status()
                    if r.status_code != 206 or len(r.content) != hi-lo+1:
                        raise RuntimeError(f'Invalid range response {r.status_code}, {len(r.content)}')
                    x = np.frombuffer(r.content, dtype='<f2').reshape(3072, 16, 96)
                    np.save(path, x)
                    print('Negative data chunk', i+1, '/32', flush=True)
                    return x
                except Exception:
                    if attempt == 3:
                        raise
                    time.sleep(2)
        with concurrent.futures.ThreadPoolExecutor(4) as pool:
            parts = list(pool.map(fetch, range(32)))
        np.save(DATA / 'negative_features.npy', np.concatenate(parts))
    print('Validation shape', np.load(DATA / 'validation_set_features.npy', mmap_mode='r').shape, flush=True)

NEGATIVE_TEXTS = [
    'Hey Jarvis.', 'Hey Nora.', 'Hey Noah.', 'Hey Nola.', 'Nova Scotia.',
    'Hey over there.', 'I know that.', 'No way.', 'Hey, no.', 'Supernova.',
    'Nova.', 'No va.', 'Hey Google.', 'Alexa.', 'Hey Siri.', 'Hey Nona.',
    'Okay, never mind.', 'They know about it.', 'Hey, nobody is here.',
    'Turn the volume down.', 'Open the browser.', 'Hello everyone.',
    'I will go over there.', 'The weather is nice today.', 'Please stop.',
    'Hey, how are you?', 'Can you hear me?', 'We need to start over.',
    'Hey November.', 'Hey Oliver.', 'Hey, no voice.', 'Hey, know what?'
]

def synthesize():
    from piper import PiperVoice, SynthesisConfig
    import onnxruntime as ort
    from piper.config import PiperConfig
    # Limit per-session threads: default ORT thread counts are slow for short TTS.
    opts = ort.SessionOptions()
    opts.intra_op_num_threads = 2
    opts.inter_op_num_threads = 1
    path = DATA / 'en_US-libritts_r-medium.onnx'
    voice = PiperVoice.load(path)
    voice.session = ort.InferenceSession(str(path), sess_options=opts, providers=['CPUExecutionProvider'])
    n_speakers = voice.config.num_speakers
    print('TTS speakers:', n_speakers, 'phonemes:', voice.phonemize('Hey Nova!'), flush=True)
    speakers = np.random.default_rng(42).permutation(n_speakers)
    train, val, test = np.split(speakers, [int(.8*n_speakers), int(.9*n_speakers)])
    metadata = {'train': train.tolist(), 'val': val.tolist(), 'test': test.tolist()}
    (DATA / 'speaker_split.json').write_text(json.dumps(metadata, indent=2))
    for split, ids in [('train', train), ('val', val), ('test', test)]:
        folder = DATA / 'speech' / split
        folder.mkdir(parents=True, exist_ok=True)
        jobs = []
        for j, speaker in enumerate(ids):
            # Standard NOH-vuh plus the user's explicit NOH-vah pronunciation.
            positives = ['Hey Nova!', '[[hˈeɪ nˈoʊvɑː]]']
            for k, text in enumerate(positives):
                jobs.append((f'positive_{speaker}_{k}.wav', int(speaker), text))
            for k in range(3):
                jobs.append((f'negative_{speaker}_{k}.wav', int(speaker), NEGATIVE_TEXTS[(j*3+k) % len(NEGATIVE_TEXTS)]))
        started = time.monotonic()
        def generate(job):
            name, speaker, text = job
            out = folder / name
            if out.exists():
                return
            config = SynthesisConfig(speaker_id=speaker, length_scale=float(RNG.uniform(.78, 1.22)), noise_scale=.667, noise_w_scale=.8)
            chunks = list(voice.synthesize(text, config))
            audio = np.concatenate([c.audio_float_array for c in chunks])
            audio = sig.resample_poly(audio, 320, 441).astype(np.float32)
            # Remove only outer near-silence, retaining phoneme boundaries.
            active = np.flatnonzero(np.abs(audio) > .008)
            if active.size:
                audio = audio[max(0, active[0]-800):min(len(audio), active[-1]+800)]
            sf.write(out, audio, 16000, subtype='PCM_16')
        with concurrent.futures.ThreadPoolExecutor(4) as pool:
            for j, _ in enumerate(pool.map(generate, jobs)):
                if (j+1) % 100 == 0:
                    print(f'Speech {split}: {j+1}/{len(jobs)}, elapsed {time.monotonic()-started:.0f}s', flush=True)

def augmented(audio):
    # Resampling changes rate and pitch; random gain, room reflections, noise,
    # and band limiting represent microphone/distance variations.
    scale = RNG.uniform(.86, 1.16)
    audio = sig.resample_poly(audio, int(100*scale), 100)
    if RNG.random() < .65:
        impulse = np.zeros(int(RNG.uniform(.05, .28)*16000), np.float32)
        impulse[0] = 1
        for _ in range(8):
            p = int(RNG.integers(150, len(impulse)))
            impulse[p] += float(RNG.uniform(-.3, .3)*np.exp(-p/2000))
        audio = sig.fftconvolve(audio, impulse)[:len(audio)]
    if len(audio) > 30000:
        audio = sig.resample(audio, 30000)
    out = np.zeros(32000, np.float32)
    end = 32000-int(RNG.uniform(.03, .23)*16000)
    end = max(end, len(audio))
    out[end-len(audio):end] = audio
    out *= RNG.uniform(.18, .95) / max(np.max(np.abs(out)), 1e-5)
    if RNG.random() < .75:
        rms = np.sqrt(np.mean(out*out))
        noise = RNG.normal(size=len(out)).astype(np.float32)
        if RNG.random() < .5:
            noise = sig.lfilter([1], [1, -.92], noise)
        noise /= max(np.std(noise), 1e-6)
        out += noise * rms / (10**(RNG.uniform(8, 30)/20))
    if RNG.random() < .3:
        out = sig.sosfilt(sig.butter(2, [150, 3800], fs=16000, btype='bandpass', output='sos'), out)
    return (np.clip(out, -1, 1)*32767).astype(np.int16)

def features():
    import onnxruntime as ort
    dll_handles = []
    nv = ROOT.parent.parent / '.venv-win' / 'Lib' / 'site-packages' / 'nvidia'
    for directory in nv.glob('*/bin'):
        os.environ['PATH'] = str(directory) + os.pathsep + os.environ.get('PATH', '')
        dll_handles.append(os.add_dll_directory(str(directory)))
    from openwakeword.utils import AudioFeatures
    existing = ROOT.parent.parent / '.venv-win' / 'Lib' / 'site-packages' / 'openwakeword' / 'resources' / 'models'
    device = 'gpu' if 'CUDAExecutionProvider' in ort.get_available_providers() else 'cpu'
    extractor = AudioFeatures(inference_framework='onnx', ncpu=4, device=device,
                              melspec_model_path=str(existing / 'melspectrogram.onnx'),
                              embedding_model_path=str(existing / 'embedding_model.onnx'))
    print('Feature execution provider:', extractor.onnx_execution_provider, flush=True)
    for split, npos, nneg in [('train', 20000, 10000), ('val', 1200, 1200), ('test', 1200, 1200)]:
        for label, count in [('positive', npos), ('negative', nneg)]:
            dest = DATA / f'{split}_{label}.npy'
            if dest.exists():
                continue
            paths = sorted((DATA / 'speech' / split).glob(f'{label}_*.wav'))
            clips = [sf.read(p, dtype='float32')[0] for p in paths]
            mmap = np.lib.format.open_memmap(dest.with_suffix('.partial.npy'), mode='w+', dtype='float32', shape=(count, 16, 96))
            for start in range(0, count, 64):
                batch = np.stack([augmented(clips[i % len(clips)]) for i in range(start, min(start+64, count))])
                emb = extractor.embed_clips(batch, batch_size=64, ncpu=4)
                assert emb.shape[1:] == (16, 96), emb.shape
                mmap[start:start+len(batch)] = emb
                if start % 1024 == 0:
                    print(f'Features {split} {label}: {start}/{count}', flush=True)
            mmap.flush()
            del mmap
            dest.with_suffix('.partial.npy').replace(dest)

def train():
    import torch
    from torch import nn
    torch.set_num_threads(4)
    torch.manual_seed(42)
    np.random.seed(42)
    pos = np.load(DATA / 'train_positive.npy')
    hard = np.load(DATA / 'train_negative.npy')
    neg = np.load(DATA / 'negative_features.npy', mmap_mode='r')
    vp = np.load(DATA / 'val_positive.npy')
    vh = np.load(DATA / 'val_negative.npy')
    raw = np.load(DATA / 'validation_set_features.npy', mmap_mode='r')
    # The published validation data is a continuous 80ms embedding sequence.
    def windows(a):
        if a.ndim == 3:
            return np.array(a, dtype=np.float32)
        return np.lib.stride_tricks.sliding_window_view(a, 16, axis=0).transpose(0, 2, 1)
    middle = len(raw)//2
    vc = windows(raw[:middle])
    tc = windows(raw[middle:])
    # Match the upstream lightweight DNN architecture, output logits for BCE.
    net = nn.Sequential(nn.Flatten(), nn.Linear(1536, 64), nn.LayerNorm(64), nn.ReLU(),
                        nn.Linear(64, 64), nn.LayerNorm(64), nn.ReLU(), nn.Linear(64, 1))
    opt = torch.optim.AdamW(net.parameters(), lr=.001, weight_decay=.01)
    def predict(x):
        net.eval()
        with torch.no_grad():
            return np.concatenate([torch.sigmoid(net(torch.from_numpy(np.array(x[i:i+1024], dtype=np.float32)))).numpy().ravel() for i in range(0, len(x), 1024)])
    best = None
    best_metric = -float('inf')
    records = []
    for step in range(8001):
        net.train()
        x = np.concatenate((pos[RNG.integers(len(pos), size=64)], hard[RNG.integers(len(hard), size=64)], np.asarray(neg[RNG.integers(len(neg), size=384)], dtype=np.float32)))
        y = torch.zeros((512, 1)); y[:64] = 1
        logits = net(torch.from_numpy(x))
        loss = nn.functional.binary_cross_entropy_with_logits(logits, y)
        opt.zero_grad(); loss.backward(); opt.step()
        for group in opt.param_groups:
            group['lr'] = .0001 + .0009*.5*(1+np.cos(np.pi*step/8000))
        if step % 400 == 0:
            ps, hs, ns = predict(vp), predict(vh), predict(vc)
            # Choose threshold using calibration set only, with <= 0.5 false
            # activation events/hour in continuous negative audio and <=1% hard negatives.
            hours = (middle*0.08/3600) if raw.ndim == 2 else (middle*1.28/3600)
            candidates = []
            for threshold in np.linspace(.05, .95, 91):
                active = ns >= threshold
                events = int(np.sum(active & ~np.r_[False, active[:-1]]))
                recall = float(np.mean(ps >= threshold))
                hr = float(np.mean(hs >= threshold))
                fph = events/hours
                metric = recall - max(0, fph-.5)*.1 - max(0, hr-.01)*2
                candidates.append((metric, recall, float(threshold), fph, hr))
            metric, recall, threshold, fph, hr = max(candidates, key=lambda c: (c[0], -c[2]))
            record = dict(step=step, loss=float(loss.detach()), recall=recall, threshold=threshold, false_events_per_hour=fph, hard_negative_rate=hr)
            records.append(record)
            print('Training', json.dumps(record), flush=True)
            if metric > best_metric:
                best_metric = metric
                best = {k:v.detach().clone() for k,v in net.state_dict().items()}
                best_record = record
    net.load_state_dict(best)
    threshold = best_record['threshold']
    output = ROOT.parent.parent / 'models' / 'wakeword'
    output.mkdir(parents=True, exist_ok=True)
    net.eval()
    exported = nn.Sequential(net, nn.Sigmoid())
    torch.onnx.export(exported, torch.zeros(1, 16, 96), str(output / 'hey_nova.onnx'), input_names=['input'], output_names=['output'], dynamic_axes={'input':{0:'batch'}, 'output':{0:'batch'}}, opset_version=17, dynamo=False)
    ps = predict(np.load(DATA / 'test_positive.npy'))
    hs = predict(np.load(DATA / 'test_negative.npy'))
    ns = predict(tc)
    active = ns >= threshold
    events = int(np.sum(active & ~np.r_[False, active[:-1]]))
    hours = len(tc)*(.08 if raw.ndim == 2 else 1.28)/3600
    report = {'phrase':'hey nova', 'pronunciation':'hey-No-Va', 'seed':20261028,
              'training_positives':len(pos), 'training_hard_negatives':len(hard), 'training_background_examples':len(neg),
              'calibration':best_record, 'threshold':threshold,
              'test':{'positive_recall':float(np.mean(ps>=threshold)), 'positive_examples':len(ps), 'hard_negative_false_rate':float(np.mean(hs>=threshold)), 'negative_hours':hours, 'negative_false_events':events, 'negative_false_events_per_hour':events/hours},
              'limitations':['Synthetic held-out speakers; personal microphone recall has not been measured.', 'Augmented examples share source utterances within each speaker split. Speaker splits are disjoint.', 'False activation counts on published embeddings; live environment performance may differ.'],
              'sources':['https://github.com/dscripka/openWakeWord', 'https://huggingface.co/rhasspy/piper-voices/tree/main/en/en_US/libritts_r/medium', 'https://huggingface.co/datasets/davidscripka/openwakeword_features'],
              'model_sha256':hashlib.sha256((output/'hey_nova.onnx').read_bytes()).hexdigest(), 'history':records}
    (output / 'training_report.json').write_text(json.dumps(report, indent=2))
    torch.save(best, DATA / 'best_classifier.pt')
    print('COMPLETE', json.dumps(report['test']), 'threshold', threshold, flush=True)

if __name__ == '__main__':
    prepare()
    synthesize()
    features()
    train()
