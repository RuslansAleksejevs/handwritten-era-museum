"""A nonlinear bottleneck with a bounded reconstruction, built anew for the museum."""
import torch
from torch import nn

class Autoencoder(nn.Module):
    def __init__(self, latent):
        super().__init__()
        if latent < 1:
            raise ValueError('latent dimension must be positive')
        self.encoder = nn.Sequential(nn.Linear(784, 256), nn.ReLU(), nn.Linear(256, latent))
        self.decoder = nn.Sequential(nn.Linear(latent, 256), nn.ReLU(), nn.Linear(256, 784), nn.Sigmoid())

    def forward(self, x):
        return self.decoder(self.encoder(x))


def errors(target, reconstructed):
    if target.shape != reconstructed.shape or target.ndim != 2:
        raise ValueError('matching image matrices required')
    return ((target - reconstructed) ** 2).mean(axis=1)
