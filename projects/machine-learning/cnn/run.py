"""A fixed CIFAR-10 protocol: train/validation selection first, test only afterwards."""
import argparse
import copy
import hashlib
import json
import platform
from pathlib import Path
import random
import time
import numpy as np
import torch
from torch.nn import functional as F
from torch.utils.data import DataLoader, TensorDataset, Subset
from cifar_data import archive_path, load_cifar, stratified_split, channel_statistics, index_digest, URL, MD5
from cnn_models import make_model, normalized, evaluate

HERE = Path(__file__).resolve().parent
MODELS = ('linear', 'historical', 'compact')


def save_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    pending = path.with_suffix('.pending.json')
    pending.write_text(json.dumps(data, indent=2, allow_nan=False) + '\n')
    pending.replace(path)


def metrics_only(result):
    return {k: result[k] for k in ('n', 'nll', 'accuracy')}


def train_one(name, seed, train, validation, mean, std, config, checkpoints):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    generator = torch.Generator().manual_seed(seed)
    loader = DataLoader(train, batch_size=config['batch_size'], shuffle=True, generator=generator)
    model = make_model(name)
    optimizer = torch.optim.AdamW(model.parameters(), lr=config['learning_rate'], weight_decay=config['weight_decay'])
    best_nll, best_epoch, best_state = float('inf'), None, None
    history = []
    started = time.perf_counter()
    for epoch in range(1, config['epochs'] + 1):
        model.train()
        total, seen = 0., 0
        for images, labels in loader:
            optimizer.zero_grad(set_to_none=True)
            logits = model(normalized(images, mean, std))
            loss = F.cross_entropy(logits, labels)
            if not torch.isfinite(loss):
                raise ValueError('Non-finite training loss')
            loss.backward()
            optimizer.step()
            total += loss.item() * len(labels)
            seen += len(labels)
        score = metrics_only(evaluate(model, validation, mean, std))
        history.append({'epoch': epoch, 'train_nll': total / seen, 'validation': score})
        if score['nll'] < best_nll:
            best_nll, best_epoch = score['nll'], epoch
            best_state = copy.deepcopy(model.state_dict())
        print(f'{name} seed={seed} epoch={epoch} val_accuracy={score["accuracy"]:.4f} val_nll={score["nll"]:.4f}', flush=True)
    model.load_state_dict(best_state)
    weights = checkpoints / f'{name}-{seed}.pt'
    torch.save(best_state, weights)
    return {'model': name, 'seed': seed, 'parameters': sum(p.numel() for p in model.parameters()),
            'best_epoch': best_epoch, 'validation_nll': best_nll, 'history': history,
            'training_seconds': time.perf_counter() - started,
            'checkpoint': weights.name, 'checkpoint_sha256': hashlib.sha256(weights.read_bytes()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--download', action='store_true')
    parser.add_argument('--output', type=Path, default=HERE / 'results')
    parser.add_argument('--epochs', type=int, default=12)
    parser.add_argument('--seeds', type=int, nargs='+', default=[2026, 2027, 2028])
    parser.add_argument('--threads', type=int, default=2)
    args = parser.parse_args()
    if args.epochs < 1 or args.threads < 1 or len(set(args.seeds)) != len(args.seeds) or min(args.seeds) < 0:
        parser.error('Positive epochs/threads and distinct nonnegative seeds required')
    torch.set_num_threads(args.threads)
    torch.use_deterministic_algorithms(True)
    archive = archive_path(args.cache, args.download)
    images, labels, test_images, test_labels = load_cifar(archive)
    train_idx, val_idx = stratified_split(labels)
    mean_np, std_np = channel_statistics(images, train_idx)
    mean, std = torch.tensor(mean_np, dtype=torch.float32), torch.tensor(std_np, dtype=torch.float32)
    full = TensorDataset(torch.from_numpy(images), torch.from_numpy(labels))
    train = Subset(full, train_idx.tolist())
    validation = DataLoader(Subset(full, val_idx.tolist()), batch_size=256)
    config = {'epochs': args.epochs, 'batch_size': 256, 'learning_rate': 0.001, 'weight_decay': 0.0001,
              'optimizer': 'AdamW', 'seeds': args.seeds, 'models': list(MODELS),
              'selection': 'minimum validation NLL within each run; no test-set selection',
              'augmentation': 'none for all models', 'device': 'cpu', 'threads': args.threads,
              'deterministic_algorithms': True, 'split_seed': 2019,
              'normalization': {'mean': mean_np.tolist(), 'std': std_np.tolist(), 'fit': 'training only'}}
    dataset = {'name': 'CIFAR-10 binary edition', 'url': URL, 'official_md5': MD5,
               'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
               'train': len(train_idx), 'validation': len(val_idx), 'test': len(test_labels),
               'train_indices_sha256': index_digest(train_idx), 'validation_indices_sha256': index_digest(val_idx)}
    # Persist the declared protocol before fitting; a run with changed settings
    # should use a separate output directory, not overwrite a published study.
    code_hashes = {name: hashlib.sha256((HERE / name).read_bytes()).hexdigest()
                   for name in ('run.py', 'cnn_models.py', 'cifar_data.py')}
    code_hashes['originals/learning/convnet.py'] = hashlib.sha256(
        (HERE.parents[2] / 'originals/learning/convnet.py').read_bytes()).hexdigest()
    plan = {'config': config, 'dataset': dataset, 'code_sha256': code_hashes}
    if (args.output / 'protocol.json').exists() or (args.output / 'metrics.json').exists():
        raise FileExistsError('Output already has a protocol/results; use a fresh --output directory')
    save_json(args.output / 'protocol.json', plan)
    checkpoints = args.cache / ('checkpoints-' + hashlib.sha256(json.dumps(plan, sort_keys=True).encode()).hexdigest()[:12])
    checkpoints.mkdir(parents=True, exist_ok=True)
    runs = []
    for seed in args.seeds:
        for name in MODELS:
            runs.append(train_one(name, seed, train, validation, mean, std, config, checkpoints))
            save_json(args.output / 'training.json', {'runs': runs})
    # The test split first enters evaluation after all fitting/selection is done.
    test = DataLoader(TensorDataset(torch.from_numpy(test_images), torch.from_numpy(test_labels)), batch_size=256)
    predictions = []
    for run in runs:
        model = make_model(run['model'])
        model.load_state_dict(torch.load(checkpoints / run['checkpoint'], weights_only=True, map_location='cpu'))
        result = evaluate(model, test, mean, std)
        run['test'] = metrics_only(result)
        predictions.append({'model': run['model'], 'seed': run['seed'], **result})
    summary = {}
    for name in MODELS:
        scores = [run['test']['accuracy'] for run in runs if run['model'] == name]
        summary[name] = {'accuracy_mean': float(np.mean(scores)),
                         'accuracy_std_across_seeds': float(np.std(scores, ddof=1)) if len(scores) > 1 else None}
    predictions_path = checkpoints / 'predictions.json'
    save_json(predictions_path, {'runs': predictions})
    report = {**plan, 'runs': runs, 'summary': summary,
              'cache_subdirectory': checkpoints.name,
              'predictions_sha256': hashlib.sha256(predictions_path.read_bytes()).hexdigest(),
              'environment': {'python': platform.python_version(), 'torch': torch.__version__,
                              'numpy': np.__version__, 'machine': platform.machine(), 'os': platform.system()},
              'limits': 'One fixed split, few seeds, small models and a short budget; not a state-of-the-art comparison.'}
    save_json(args.output / 'metrics.json', report)
    from plot_results import render
    render(report, predictions, test_images, args.output)
    print(json.dumps(summary, indent=2), flush=True)


if __name__ == '__main__':
    main()
