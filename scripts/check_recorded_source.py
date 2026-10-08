"""Check that the archived reconstruction source matches the saved ML results."""
import hashlib
import json
from pathlib import Path

from check_originals import unique_pairs


def read(path):
    return json.loads(path.read_text(), object_pairs_hook=unique_pairs)


def main():
    ml = Path(__file__).resolve().parents[1] / 'projects/machine-learning'
    archive = ml / 'recorded-source'
    entries = read(archive / 'manifest.json')
    indexed = {}
    for entry in entries:
        name = entry['path']
        if name in indexed:
            raise ValueError('Duplicate archived path: ' + name)
        actual = hashlib.sha256((archive / name).read_bytes()).hexdigest()
        if actual != entry['sha256']:
            raise ValueError('Archived source changed: ' + name)
        indexed[name] = actual
    recorded = 0
    for chapter in ('segmentation', 'transfer', 'autoencoder', 'forecasting'):
        metrics = read(ml / chapter / 'results/metrics.json')
        for name, digest in metrics['code_sha256'].items():
            if indexed.get(f'{chapter}/{name}') != digest:
                raise ValueError(f'Recorded experiment source is missing or changed: {chapter}/{name}')
            recorded += 1
    print(f'PASS: {recorded} recorded experiment sources and {len(indexed)-recorded} supporting source retain their fingerprints')


if __name__ == '__main__':
    main()
