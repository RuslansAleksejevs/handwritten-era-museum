"""Read the pinned CIFAR-10 binary edition without executing pickle or extracting files."""
from pathlib import Path
import hashlib
import tarfile
import urllib.request
import numpy as np

URL = 'https://www.cs.toronto.edu/~kriz/cifar-10-binary.tar.gz'
MD5 = 'c32a1d4ab5d03f1284b67883e8d87530'
CLASSES = ('airplane', 'automobile', 'bird', 'cat', 'deer', 'dog', 'frog', 'horse', 'ship', 'truck')


def archive_path(cache, download=False):
    path = Path(cache) / 'cifar-10-binary.tar.gz'
    if not path.exists():
        if not download:
            raise FileNotFoundError('CIFAR-10 is absent; pass --download to fetch the official archive')
        path.parent.mkdir(parents=True, exist_ok=True)
        pending = path.with_suffix('.pending')
        try:
            with urllib.request.urlopen(URL, timeout=60) as source, pending.open('wb') as dest:
                while chunk := source.read(1024 * 1024):
                    dest.write(chunk)
            if hashlib.md5(pending.read_bytes()).hexdigest() != MD5:
                raise ValueError('Downloaded CIFAR-10 differs from the official checksum')
            pending.replace(path)
        finally:
            pending.unlink(missing_ok=True)
    if hashlib.md5(path.read_bytes()).hexdigest() != MD5:
        raise ValueError('CIFAR-10 archive checksum mismatch')
    return path


def decode_batch(raw, count=10000):
    if len(raw) != count * 3073:
        raise ValueError('Wrong CIFAR binary batch size')
    rows = np.frombuffer(raw, dtype=np.uint8).reshape(count, 3073)
    labels = rows[:, 0].astype(np.int64)
    if np.any(labels >= len(CLASSES)):
        raise ValueError('Invalid CIFAR label')
    return rows[:, 1:].reshape(count, 3, 32, 32).copy(), labels


def load_cifar(path):
    train_x, train_y = [], []
    with tarfile.open(path, 'r:gz') as archive:
        for i in range(1, 7):
            name = f'data_batch_{i}.bin' if i <= 5 else 'test_batch.bin'
            member = archive.getmember('cifar-10-batches-bin/' + name)
            if not member.isfile() or member.size != 10000 * 3073:
                raise ValueError('Unexpected CIFAR archive member')
            with archive.extractfile(member) as source:
                x, y = decode_batch(source.read())
            if i <= 5:
                train_x.append(x)
                train_y.append(y)
            else:
                test_x, test_y = x, y
    return np.concatenate(train_x), np.concatenate(train_y), test_x, test_y


def stratified_split(labels, validation_per_class=500, seed=2019):
    """Indices refer only to the official training partition."""
    labels = np.asarray(labels)
    if labels.ndim != 1 or validation_per_class < 1:
        raise ValueError('Expected labels and a positive validation size')
    rng = np.random.default_rng(seed)
    train, validation = [], []
    for label in np.unique(labels):
        indices = rng.permutation(np.flatnonzero(labels == label))
        if len(indices) <= validation_per_class:
            raise ValueError('Every class needs both training and validation examples')
        validation.extend(indices[:validation_per_class])
        train.extend(indices[validation_per_class:])
    return np.sort(train), np.sort(validation)


def channel_statistics(images, indices):
    """Fit channel scaling on training examples only, in bounded chunks."""
    if len(indices) == 0:
        raise ValueError('No training images')
    total = np.zeros(3, dtype=np.float64)
    squares = total.copy()
    pixels = 0
    for start in range(0, len(indices), 256):
        x = images[indices[start:start + 256]].astype(np.float64) / 255
        total += x.sum(axis=(0, 2, 3))
        squares += (x * x).sum(axis=(0, 2, 3))
        pixels += len(x) * 32 * 32
    mean = total / pixels
    std = np.sqrt(np.maximum(squares / pixels - mean * mean, 0))
    if np.any(std <= 0):
        raise ValueError('Constant training channel')
    return mean, std


def index_digest(indices):
    return hashlib.sha256(np.asarray(indices, dtype='<i8').tobytes()).hexdigest()
