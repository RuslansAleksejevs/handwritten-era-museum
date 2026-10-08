"""Re-evaluate saved weights and compare them with the recorded test results."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import torch
from torch.utils.data import DataLoader, TensorDataset
from cifar_data import archive_path, load_cifar, stratified_split, channel_statistics, index_digest
from cnn_models import make_model, evaluate


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON key: ' + key)
        result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--results', type=Path, default=Path(__file__).resolve().parent / 'results')
    args = parser.parse_args()
    report = json.loads((args.results / 'metrics.json').read_text(), object_pairs_hook=unique)
    here = Path(__file__).resolve().parent
    for name, digest in report['code_sha256'].items():
        path = here.parents[2] / name if name.startswith('originals/') else here / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError('Experiment source differs: ' + name)
    torch.set_num_threads(report['config']['threads'])
    torch.use_deterministic_algorithms(True)
    archive = archive_path(args.cache)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != report['dataset']['sha256']:
        raise ValueError('Dataset SHA-256 changed')
    images, labels, test_images, test_labels = load_cifar(archive)
    train_idx, val_idx = stratified_split(labels)
    for key, value in [('train_indices_sha256', train_idx), ('validation_indices_sha256', val_idx)]:
        if index_digest(value) != report['dataset'][key]:
            raise ValueError('Split differs')
    mean, std = channel_statistics(images, train_idx)
    recorded = report['config']['normalization']
    np.testing.assert_allclose(mean, recorded['mean'], atol=1e-12, rtol=0)
    np.testing.assert_allclose(std, recorded['std'], atol=1e-12, rtol=0)
    mean, std = torch.tensor(mean, dtype=torch.float32), torch.tensor(std, dtype=torch.float32)
    test = DataLoader(TensorDataset(torch.from_numpy(test_images), torch.from_numpy(test_labels)), batch_size=256)
    cache = args.cache / report['cache_subdirectory']
    raw_predictions = (cache / 'predictions.json').read_bytes()
    if hashlib.sha256(raw_predictions).hexdigest() != report['predictions_sha256']:
        raise ValueError('Prediction artifact changed')
    predictions = json.loads(raw_predictions, object_pairs_hook=unique)['runs']
    for run in report['runs']:
        weights = cache / run['checkpoint']
        if hashlib.sha256(weights.read_bytes()).hexdigest() != run['checkpoint_sha256']:
            raise ValueError('Checkpoint changed')
        best = min(run['history'], key=lambda row: row['validation']['nll'])
        if run['best_epoch'] != best['epoch'] or run['validation_nll'] != best['validation']['nll']:
            raise ValueError('Checkpoint was not selected by minimum validation NLL')
        model = make_model(run['model'])
        model.load_state_dict(torch.load(weights, weights_only=True, map_location='cpu'))
        result = evaluate(model, test, mean, std)
        expected = next(p for p in predictions if (p['model'], p['seed']) == (run['model'], run['seed']))
        if result['predictions'] != expected['predictions'] or result['targets'] != expected['targets']:
            raise ValueError('Predictions differ from the saved experiment')
        for metric in ('n', 'nll', 'accuracy'):
            np.testing.assert_allclose(result[metric], run['test'][metric], atol=1e-7, rtol=0)
        np.testing.assert_allclose(result['confidence'], expected['confidence'], atol=1e-7, rtol=0)
        print(f'PASS: {run["model"]}, seed {run["seed"]}', flush=True)


if __name__ == '__main__':
    main()
