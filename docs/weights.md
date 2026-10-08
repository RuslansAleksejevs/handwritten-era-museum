[← Run the collection](reproduce.md)

# Saved weights

The [`museum-weights-v1.0.0.tar.gz` release bundle](https://github.com/RuslansAleksejevs/handwritten-era-museum/releases/tag/v1.0.0) contains 17 model checkpoints,
CNN prediction evidence and the relevant notices. The [manifest](weights-manifest.json)
records every payload's SHA-256. Dataset archives are separate; the chapter
scripts fetch their pinned sources. A verified local backup also preserves those
archives, so the saved results do not depend on a temporary cache surviving.

With the matching bundle and `SHA256SUMS` downloaded from the same release:

```sh
shasum -a 256 -c SHA256SUMS
mkdir -p /tmp/museum-weights
tar -xzf museum-weights-v1.0.0.tar.gz -C /tmp/museum-weights
```

Use `/tmp/museum-weights/cnn`, `/tmp/museum-weights/music` and
`/tmp/museum-weights/extensions` as the corresponding chapter caches.
Keep a separate copy when retraining: the runners write new checkpoints.
To fetch CIFAR-10 and verify the saved CNN results without retraining:

```sh
PYTHONPATH=projects/machine-learning/cnn python -c \
  'from cifar_data import archive_path; archive_path("/tmp/museum-weights/cnn", download=True)'
python projects/machine-learning/cnn/verify.py --cache /tmp/museum-weights/cnn
```

The fresh check reproduced all nine CNN prediction sets from the saved weights.
Retraining music, segmentation and autoencoders also reproduced their seven
checkpoint files. This is a same-platform result; other libraries or hardware
may introduce floating-point differences.
