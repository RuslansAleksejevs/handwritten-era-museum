"""SqueezeNet 1.1 feature graph, adapted from torchvision (BSD; see notice)."""
import hashlib
from pathlib import Path
import sys
import numpy as np
from PIL import Image
import torch
from torch import nn
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'autoencoder'))
from fashion_data import fetch

WEIGHT_URL = 'https://download.pytorch.org/models/squeezenet1_1-b8a52dc0.pth'
WEIGHT_SHA = 'b8a52dc049b60e4b6ab68ad0df457362afab8b6304b2febdc1650a5dab4d7e7b'

class Fire(nn.Module):
    def __init__(self, incoming, squeeze, expand):
        super().__init__()
        self.squeeze = nn.Conv2d(incoming, squeeze, 1)
        self.expand1x1 = nn.Conv2d(squeeze, expand, 1)
        self.expand3x3 = nn.Conv2d(squeeze, expand, 3, padding=1)
    def forward(self, x):
        x = self.squeeze(x).relu()
        return torch.cat([self.expand1x1(x).relu(), self.expand3x3(x).relu()], dim=1)

class SqueezeFeatures(nn.Module):
    def __init__(self):
        super().__init__()
        def pool(): return nn.MaxPool2d(3, 2, ceil_mode=True)
        self.features = nn.Sequential(nn.Conv2d(3,64,3,stride=2), nn.ReLU(), pool(),
            Fire(64,16,64), Fire(128,16,64), pool(), Fire(128,32,128), Fire(256,32,128), pool(),
            Fire(256,48,192), Fire(384,48,192), Fire(384,64,256), Fire(512,64,256))
        for layer in self.modules():
            if isinstance(layer, nn.Conv2d):
                nn.init.kaiming_uniform_(layer.weight); nn.init.zeros_(layer.bias)
    def forward(self, x):
        return self.features(x).mean(dim=(-2,-1))


def frozen_model(cache, pretrained, download=False):
    torch.manual_seed(2026)
    model = SqueezeFeatures()
    if pretrained:
        path = fetch(WEIGHT_URL,Path(cache)/'squeezenet1_1-b8a52dc0.pth',WEIGHT_SHA,download,10_000_000)
        weights = torch.load(path,map_location='cpu',weights_only=True)
        model.load_state_dict({k:v for k,v in weights.items() if k.startswith('features.')},strict=True)
    model.requires_grad_(False); model.eval()
    return model


def preprocess(images):
    """Official V1 resize 256 / center crop 224 / ImageNet normalization."""
    result = []
    for image in images:
        pil = Image.fromarray(image.transpose(1,2,0)).resize((256,256),Image.Resampling.BILINEAR)
        result.append(np.asarray(pil.crop((16,16,240,240))).transpose(2,0,1).copy())
    x = torch.from_numpy(np.stack(result)).float()/255
    return (x-torch.tensor([.485,.456,.406])[None,:,None,None])/torch.tensor([.229,.224,.225])[None,:,None,None]


@torch.no_grad()
def features(model, images):
    result=[]
    for start in range(0,len(images),32):
        result.append(model(preprocess(images[start:start+32])).numpy())
        if start % 1024 == 0: print('features',start,'/',len(images),flush=True)
    return np.concatenate(result)
