"""Check that displayed historical code still has the recorded source bytes."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'originals'


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'duplicate JSON key: {key}')
        result[key] = value
    return result


def main():
    manifest = json.loads((ROOT/'manifest.json').read_text(), object_pairs_hook=unique_pairs)
    for exhibit in manifest:
        path = ROOT / exhibit['path']
        if hashlib.sha256(path.read_bytes()).hexdigest() != exhibit['excerpt_sha256']:
            raise ValueError(f'historical text changed: {exhibit["path"]}')
    print(f'PASS: {len(manifest)} historical code exhibits retain their recorded fingerprints')


if __name__ == '__main__':
    main()
