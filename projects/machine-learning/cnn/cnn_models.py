"""One unchanged historical architecture and two explicitly modern comparators."""
from pathlib import Path
import runpy
import torch
from torch import nn
from torch.nn import functional as F


class CompactCNN(nn.Module):
    def __init__(self):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv2d(3, 16, 3, padding=1), nn.ReLU(), nn.MaxPool2d(2),
            nn.Conv2d(16, 32, 3, padding=1), nn.ReLU(), nn.MaxPool2d(2),
            nn.Conv2d(32, 64, 3, padding=1), nn.ReLU(), nn.MaxPool2d(2),
        )
        self.classifier = nn.Sequential(nn.Flatten(), nn.Linear(64 * 4 * 4, 64), nn.ReLU(), nn.Linear(64, 10))

    def forward(self, images):
        return self.classifier(self.features(images))


def make_model(name):
    if name == 'linear':
        return nn.Sequential(nn.Flatten(), nn.Linear(3 * 32 * 32, 10))
    if name == 'compact':
        return CompactCNN()
    if name == 'historical':
        original = Path(__file__).resolve().parents[3] / 'originals/learning/convnet.py'
        # The preserved cell deliberately has no imports. Supply its original
        # notebook globals rather than modifying the historical source.
        scope = runpy.run_path(str(original), init_globals={'nn': nn, 'F': F})
        return scope['ConvNet']()
    raise ValueError('Unknown model: ' + name)


def normalized(images, mean, std):
    return (images.to(torch.float32) / 255 - mean[None, :, None, None]) / std[None, :, None, None]


def evaluate(model, loader, mean, std):
    """Example-weighted NLL, with evaluation mode restored even on an error."""
    was_training = model.training
    model.eval()
    loss_sum, count, correct = 0., 0, 0
    predictions, confidences, targets = [], [], []
    try:
        with torch.inference_mode():
            for images, labels in loader:
                logits = model(normalized(images, mean, std))
                loss_sum += F.cross_entropy(logits, labels, reduction='sum').item()
                confidence, prediction = logits.softmax(dim=1).max(dim=1)
                count += len(labels)
                correct += (prediction == labels).sum().item()
                predictions.extend(prediction.tolist())
                confidences.extend(confidence.tolist())
                targets.extend(labels.tolist())
    finally:
        model.train(was_training)
    if not count:
        raise ValueError('Cannot evaluate an empty dataset')
    return {'n': count, 'nll': loss_sum / count, 'accuracy': correct / count,
            'predictions': predictions, 'confidence': confidences, 'targets': targets}
