"""Compact U-Net and a color-only baseline. Metrics aggregate pixels before division."""

import numpy as np
import torch
from torch import nn
from torch.nn import functional as F


def block(incoming, outgoing):
    return nn.Sequential(
        nn.Conv2d(incoming, outgoing, 3, padding=1),
        nn.GroupNorm(4, outgoing),
        nn.ReLU(),
        nn.Conv2d(outgoing, outgoing, 3, padding=1),
        nn.GroupNorm(4, outgoing),
        nn.ReLU(),
    )


class SmallUNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.first = block(3, 8)
        self.second = block(8, 16)
        self.bottom = block(16, 32)
        self.up2 = block(48, 16)
        self.up1 = block(24, 8)
        self.output = nn.Conv2d(8, 5, 1)

    def forward(self, x):
        a = self.first(x)
        b = self.second(F.max_pool2d(a, 2))
        c = self.bottom(F.max_pool2d(b, 2))
        d = self.up2(
            torch.cat(
                [
                    F.interpolate(
                        c, size=b.shape[-2:], mode="bilinear", align_corners=False
                    ),
                    b,
                ],
                1,
            )
        )
        return self.output(
            self.up1(
                torch.cat(
                    [
                        F.interpolate(
                            d, size=a.shape[-2:], mode="bilinear", align_corners=False
                        ),
                        a,
                    ],
                    1,
                )
            )
        )


class PixelClassifier(nn.Module):
    def __init__(self):
        super().__init__()
        self.layers = nn.Sequential(nn.Conv2d(3, 24, 1), nn.ReLU(), nn.Conv2d(24, 5, 1))

    def forward(self, x):
        return self.layers(x)


def confusion(target, prediction, classes=5):
    target, prediction = np.asarray(target), np.asarray(prediction)
    if (
        target.shape != prediction.shape
        or not np.isin(target, range(classes)).all()
        or not np.isin(prediction, range(classes)).all()
    ):
        raise ValueError("matching class masks required")
    return np.bincount(
        (target.ravel() * classes + prediction.ravel()), minlength=classes * classes
    ).reshape(classes, classes)


def scores(matrix):
    matrix = np.asarray(matrix, dtype=np.float64)
    tp = np.diag(matrix)
    union = matrix.sum(0) + matrix.sum(1) - tp
    valid = union > 0
    iou = np.divide(tp, union, out=np.zeros_like(tp), where=valid)
    return {
        "pixel_accuracy": float(tp.sum() / matrix.sum()),
        "mean_iou": float(iou[valid].mean()),
        "per_class_iou": [float(v) if ok else None for v, ok in zip(iou, valid)],
        "confusion_matrix": matrix.astype(int).tolist(),
    }
