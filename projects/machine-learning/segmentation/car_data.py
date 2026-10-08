"""Read only paired PNG members from the recovered teaching dataset, without extraction."""

import hashlib
import io
import json
from pathlib import Path
import re
import sys
import urllib.request
from urllib.parse import quote
import zipfile
import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "autoencoder"))
from fashion_data import fetch

PUBLIC_URL = "https://disk.yandex.com/d/plvsPQhbb1vvEw"
ARCHIVE_SHA = "c73ff64ab732b28e9aaad4ec185df238221572b30291a739b2e59dfd67b6ca66"
SIZE = 96
CLASSES = ["background", "car", "wheel", "lights", "window"]


def source_group(name):
    # Capture date is a deliberately conservative proxy; true scene IDs are absent.
    if match := re.match(r"IMG_(\d{8})_", name):
        return "capture-day-" + match[1]
    if re.fullmatch(r"im\d+\.png", name):
        return "numbered-im-series"
    if name.startswith("image%20"):
        return "numbered-image-series"
    return name


def assign_groups(names, hashes, seed=2026):
    parent = list(range(len(names)))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    seen_group, seen_hash = {}, {}
    for i, (name, sha) in enumerate(zip(names, hashes)):
        for key, seen in [(source_group(name), seen_group), (sha, seen_hash)]:
            if key in seen:
                parent[root(i)] = root(seen[key])
            else:
                seen[key] = i
    groups = {}
    for i in range(len(names)):
        groups.setdefault(root(i), []).append(i)
    rng = np.random.default_rng(seed)
    shuffled = list(groups.values())
    rng.shuffle(shuffled)
    shuffled.sort(key=lambda g: -len(g))
    targets = np.array([0.7, 0.15, 0.15]) * len(names)
    sizes = np.zeros(3)
    splits = [""] * len(names)
    family = [""] * len(names)
    for group in shuffled:
        slot = int(np.argmin(sizes / targets))
        sizes[slot] += len(group)
        for index in group:
            splits[index] = ["train", "validation", "test"][slot]
            family[index] = min(names[i] for i in group)
    if any(size == 0 for size in sizes):
        raise ValueError("need at least three source groups")
    return splits, family


def load_cars(cache, download=False):
    path = Path(cache) / "car-segmentation.zip"
    url = PUBLIC_URL
    if not path.exists() and download:
        endpoint = (
            "https://cloud-api.yandex.net/v1/disk/public/resources/download?public_key="
            + quote(PUBLIC_URL, safe="")
        )
        with urllib.request.urlopen(endpoint, timeout=30) as response:
            url = json.load(response)["href"]
    path = fetch(url, path, ARCHIVE_SHA, download)
    images, masks, entries = [], [], []
    with zipfile.ZipFile(path) as archive:
        names = sorted(
            n
            for n in archive.namelist()
            if re.fullmatch(r"car-segmentation/images/[^/]+\.png", n)
        )
        if len(names) != 211:
            raise ValueError("expected 211 teaching images")
        for name in names:
            raw = archive.read(name)
            mask_raw = archive.read(name.replace("/images/", "/masks/"))
            with Image.open(io.BytesIO(raw)) as source:
                rgb = source.convert("RGB")
                pixel_hash = hashlib.sha256(rgb.tobytes()).hexdigest()
                image = np.asarray(
                    rgb.resize((SIZE, SIZE), Image.Resampling.BILINEAR)
                ).copy()
            with Image.open(io.BytesIO(mask_raw)) as source:
                mask = np.asarray(
                    source.resize((SIZE, SIZE), Image.Resampling.NEAREST)
                ).copy()
            if mask.ndim != 2 or not np.isin(mask, np.arange(5)).all():
                raise ValueError("invalid class mask")
            images.append(image.transpose(2, 0, 1))
            masks.append(mask.astype(np.int64))
            entries.append(
                {
                    "id": Path(name).name,
                    "source_sha256": hashlib.sha256(raw).hexdigest(),
                    "mask_sha256": hashlib.sha256(mask_raw).hexdigest(),
                    "pixels_sha256": pixel_hash,
                }
            )
    splits, families = assign_groups(
        [e["id"] for e in entries], [e["pixels_sha256"] for e in entries]
    )
    for e, s, f in zip(entries, splits, families):
        e.update(split=s, family=f)
    return np.stack(images), np.stack(masks), entries
