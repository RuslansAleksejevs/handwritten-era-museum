"""Train, validate, freeze, evaluate once, and render a reproducible music study."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import platform
import random
import tempfile
import subprocess
import sys
import time
import numpy as np
import torch
from torch import nn
from corpus import prepare, manifest
from model import MelodyGRU, Markov, batch, event_losses


def evaluate(model, pieces):
    model.eval()
    reports = []
    with torch.no_grad():
        for piece in pieces:
            packed = batch([piece])
            loss, pitch, duration, mask = event_losses(model, packed)
            y = packed[1]
            reports.append({'id': piece['id'], 'family': piece['family'],
                'n': int(mask.sum()), 'nll': float(loss.mean()),
                'pitch_accuracy': float((pitch.argmax(-1)[mask] == y[..., 0][mask]).float().mean()),
                'duration_accuracy_given_pitch': float((duration.argmax(-1)[mask] == y[..., 1][mask]).float().mean())})
    return aggregate(reports)


def aggregate(reports):
    count = sum(p['n'] for p in reports)
    return {'nll': sum(p['nll']*p['n'] for p in reports)/count,
            'piece_mean_nll': float(np.mean([p['nll'] for p in reports])),
            'events': count, 'pieces': reports}


def evaluate_markov(model, pieces):
    return aggregate([{'id': p['id'], 'family': p['family'],
                       'n': len(p['events'])-8, 'nll': float(model.score(p).mean())} for p in pieces])


def train(train_pieces, validation, seed, epochs, cache, config):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    model = MelodyGRU(**config)
    optimizer = torch.optim.AdamW(model.parameters(), lr=.002, weight_decay=.01)
    best, best_epoch, best_state, history = float('inf'), 0, None, []
    began = time.monotonic()
    for epoch in range(1, epochs + 1):
        model.train()
        order = list(train_pieces)
        random.shuffle(order)
        total, n = 0., 0
        for start in range(0, len(order), 24):
            packed = batch(order[start:start+24])
            optimizer.zero_grad()
            values, _, _, _ = event_losses(model, packed)
            loss = values.mean()
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), 1.)
            optimizer.step()
            total += float(values.detach().sum())
            n += len(values)
        val = evaluate(model, validation)['nll']
        history.append({'epoch': epoch, 'train_nll': total/n, 'validation_nll': val})
        if val < best - 1e-5:
            best, best_epoch, best_state = val, epoch, copy.deepcopy(model.state_dict())
        if epoch == 1 or epoch % 5 == 0:
            print(f'seed {seed} epoch {epoch}: train {total/n:.4f}, validation {val:.4f}', flush=True)
        if epoch - best_epoch >= 12:
            break
    model.load_state_dict(best_state)
    checkpoint = Path(cache)/f'gru-{seed}.pt'
    torch.save({'config': config, 'state_dict': best_state}, checkpoint)
    return model, {'seed': seed, 'best_epoch': best_epoch, 'validation_nll': best,
                   'parameters': sum(p.numel() for p in model.parameters()),
                   'seconds': time.monotonic()-began, 'history': history,
                   'checkpoint_sha256': hashlib.sha256(checkpoint.read_bytes()).hexdigest()}


def paired_bootstrap(neural, baseline, samples=5000):
    # Resample whole hymn families, preserving dependence among related settings.
    families = {}
    for a, b in zip(neural['pieces'], baseline['pieces']):
        assert a['id'] == b['id'] and a['n'] == b['n']
        families.setdefault(a['family'], []).append((a['nll']-b['nll'], a['n']))
    totals = np.array([[sum(d*n for d,n in group), sum(n for _,n in group)]
                       for group in families.values()])
    rng = np.random.default_rng(2048)
    selected = totals[rng.integers(0, len(totals), (samples, len(totals)))].sum(1)
    delta = selected[:, 0]/selected[:, 1]
    return {'unit': 'hymn family', 'samples': samples, 'seed': 2048,
            'gru_minus_markov_95_percent_interval': np.quantile(delta, [.025, .975]).tolist()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, default=Path(tempfile.gettempdir())/'museum-music')
    parser.add_argument('--output', type=Path, default=Path(__file__).parent/'results')
    parser.add_argument('--epochs', type=int, default=80)
    parser.add_argument('--seeds', type=int, nargs='+', default=[2022, 2023, 2024])
    parser.add_argument('--skip-test', action='store_true', help='validation-only development run')
    args = parser.parse_args()
    if args.epochs < 1 or not args.seeds or len(set(args.seeds)) != len(args.seeds):
        parser.error('positive epochs and distinct seeds required')
    torch.set_num_threads(2)
    torch.use_deterministic_algorithms(True)
    data = prepare(args.cache)
    splits = {s: [p for p in data['pieces'] if p['split'] == s]
              for s in ('train', 'validation', 'test')}
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output/'split.json').write_text(json.dumps(manifest(data), ensure_ascii=False, indent=2)+'\n')
    candidates = []
    for order in (1, 2, 3):
        for strength in (1., 5., 20.):
            baseline = Markov(order, strength).fit(splits['train'])
            val = evaluate_markov(baseline, splits['validation'])['nll']
            candidates.append({'order': order, 'strength': strength, 'validation_nll': val})
    best_baseline = min(candidates, key=lambda c: c['validation_nll'])
    baseline = Markov(best_baseline['order'], best_baseline['strength']).fit(splits['train'])
    print('selected baseline', best_baseline, flush=True)
    config = dict(hidden=128, layers=2, dropout=.2)
    models, runs = [], []
    for seed in args.seeds:
        model, report = train(splits['train'], splits['validation'], seed, args.epochs, args.cache, config)
        models.append(model)
        runs.append(report)
    selected = min(range(len(runs)), key=lambda i: runs[i]['validation_nll'])
    report = {'schema': 1, 'corpus_sha256': data['pieces_sha256'], 'config': config,
        'maximum_epochs': args.epochs, 'seeds': args.seeds, 'selected_seed': args.seeds[selected],
        'selection': 'minimum validation event NLL; test never selects models or sampling',
        'baseline_candidates': candidates, 'selected_baseline': best_baseline, 'runs': runs,
        'recipe': {'optimizer': 'AdamW', 'learning_rate': .002, 'weight_decay': .01,
                   'batch_pieces': 24, 'gradient_clip': 1., 'patience_epochs': 12,
                   'prompt_events': 8, 'augmentation': 'none', 'device': 'CPU'},
        'environment': {'python': platform.python_version(), 'torch': torch.__version__,
                        'numpy': np.__version__, 'platform': platform.platform(), 'threads': 2}}
    if not args.skip_test:
        # No optimizer or model selection below this point.
        report['markov_test'] = evaluate_markov(baseline, splits['test'])
        for model, run in zip(models, runs):
            run['test'] = evaluate(model, splits['test'])
        report['paired_bootstrap'] = paired_bootstrap(runs[selected]['test'], report['markov_test'])
        report['test_nll_mean'] = float(np.mean([r['test']['nll'] for r in runs]))
        report['test_nll_std'] = float(np.std([r['test']['nll'] for r in runs], ddof=1)) if len(runs)>1 else None
    import music21
    report['environment']['music21'] = music21.__version__
    (args.output/'metrics.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'selected_seed': report['selected_seed'],
                      'validation': runs[selected]['validation_nll'],
                      'test': report.get('test_nll_mean'),
                      'baseline_test': report.get('markov_test', {}).get('nll')}), flush=True)

    if not args.skip_test:
        subprocess.run([sys.executable, str(Path(__file__).with_name('render.py')),
                        '--cache', str(args.cache), '--output', str(args.output)], check=True)


if __name__ == '__main__':
    main()
