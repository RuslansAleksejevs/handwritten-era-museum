"""Pinned Fashion-MNIST IDX files. Data stays in an explicit external cache."""

import gzip
import hashlib
from pathlib import Path
import struct
import urllib.request
import numpy as np

REVISION = "b2617bb6d3ffa2e429640350f613e3291e10b141"
SOURCE = f"https://raw.githubusercontent.com/zalandoresearch/fashion-mnist/{REVISION}/data/fashion/"
HASHES = {
    "train-images-idx3-ubyte.gz": "3aede38d61863908ad78613f6a32ed271626dd12800ba2636569512369268a84",
    "train-labels-idx1-ubyte.gz": "a04f17134ac03560a47e3764e11b92fc97de4d1bfaf8ba1a3aa29af54cc90845",
    "t10k-images-idx3-ubyte.gz": "346e55b948d973a97e58d2351dde16a484bd415d4595297633bb08f03db6a073",
    "t10k-labels-idx1-ubyte.gz": "67da17c76eaffca5446c3361aaab5c3cd6d1c2608764d35dfb1850b086bf8dd5",
}


def fetch(url, path, expected, download=False, maximum=600_000_000):
    path = Path(path)
    if not path.exists():
        if not download:
            raise FileNotFoundError(f"{path.name} missing: pass --download")
        path.parent.mkdir(parents=True, exist_ok=True)
        pending = path.with_suffix(path.suffix + ".pending")
        try:
            count = 0
            with urllib.request.urlopen(url, timeout=90) as source, pending.open(
                "wb"
            ) as dest:
                while chunk := source.read(1024 * 1024):
                    count += len(chunk)
                    if count > maximum:
                        raise ValueError("download exceeds size limit")
                    dest.write(chunk)
            if hashlib.sha256(pending.read_bytes()).hexdigest() != expected:
                raise ValueError("download checksum mismatch")
            pending.replace(path)
        finally:
            pending.unlink(missing_ok=True)
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError("cached file checksum mismatch")
    return path


def read_idx(data):
    if len(data) < 8:
        raise ValueError("truncated IDX")
    magic, count = struct.unpack(">II", data[:8])
    if magic == 2049 and len(data) == 8 + count:
        return np.frombuffer(data, dtype=np.uint8, offset=8).copy()
    if magic == 2051 and len(data) >= 16:
        rows, columns = struct.unpack(">II", data[8:16])
        if (rows, columns) == (28, 28) and len(data) == 16 + count * rows * columns:
            return (
                np.frombuffer(data, dtype=np.uint8, offset=16)
                .reshape(count, 784)
                .copy()
            )
    raise ValueError("invalid IDX structure")


def load_fashion(cache, download=False):
    arrays = [
        read_idx(
            gzip.decompress(
                fetch(
                    SOURCE + name, Path(cache) / name, sha, download, maximum=40_000_000
                ).read_bytes()
            )
        )
        for name, sha in HASHES.items()
    ]
    if [len(a) for a in arrays] != [60000, 60000, 10000, 10000]:
        raise ValueError("unexpected Fashion-MNIST counts")
    return arrays


def selected_split(
    images, labels, test_images, seed=2026, train_per_class=1200, val_per_class=200
):
    """Unique images only; never place a byte-identical official test image in training."""
    heldout = {row.tobytes() for row in test_images}
    rng = np.random.default_rng(seed)
    seen, train, validation = set(heldout), [], []
    removed = 0
    for label in range(10):
        candidates = rng.permutation(np.flatnonzero(labels == label))
        selected = []
        for index in candidates:
            key = images[index].tobytes()
            if key in seen:
                removed += 1
                continue
            seen.add(key)
            selected.append(int(index))
            if len(selected) == train_per_class + val_per_class:
                break
        if len(selected) != train_per_class + val_per_class:
            raise ValueError("insufficient unique examples")
        train.extend(selected[:train_per_class])
        validation.extend(selected[train_per_class:])
    return np.sort(train), np.sort(validation), removed
