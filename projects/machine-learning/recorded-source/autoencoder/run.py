"""Fashion-MNIST compression: dimensions 2 and 30, autoencoder versus PCA."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import platform
import time
import numpy as np
import torch
from sklearn.decomposition import PCA
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from fashion_data import load_fashion, selected_split, HASHES, REVISION
from ae_model import Autoencoder, errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, default=Path('/tmp/museum-ml-extensions'))
    parser.add_argument('--output', type=Path, default=Path(__file__).parent/'results')
    parser.add_argument('--download', action='store_true')
    parser.add_argument('--epochs', type=int, default=30)
    args = parser.parse_args()
    if args.epochs < 1: parser.error('positive epochs required')
    torch.set_num_threads(2); torch.use_deterministic_algorithms(True)
    images, labels, test_images, test_labels = load_fashion(args.cache, args.download)
    train, validation, removed = selected_split(images, labels, test_images)
    x = torch.from_numpy(images[train].astype(np.float32)/255)
    v = torch.from_numpy(images[validation].astype(np.float32)/255)
    t = torch.from_numpy(test_images.astype(np.float32)/255)
    args.output.mkdir(parents=True, exist_ok=True)
    split = {'train_indices': train.tolist(), 'validation_indices': validation.tolist(),
             'test': 'all 10000 official test images', 'exact_duplicate_candidates_skipped': removed,
             'rule': 'per-class permutation seed 2026; exclude exact official-test matches and repeated selected images'}
    (args.output/'split.json').write_text(json.dumps(split, indent=2)+'\n')
    selected, candidates, models = {}, [], {}
    began = time.monotonic()
    for dimension in [2, 30]:
        torch.manual_seed(2026)
        model = Autoencoder(dimension)
        optimizer = torch.optim.Adam(model.parameters(), lr=.001)
        best, state, history = float('inf'), None, []
        for epoch in range(1, args.epochs+1):
            model.train(); order = torch.randperm(len(x)); total = 0.
            for batch in order.split(256):
                optimizer.zero_grad(); loss = (model(x[batch])-x[batch]).square().mean()
                loss.backward(); optimizer.step(); total += float(loss.detach())*len(batch)
            model.eval()
            with torch.no_grad(): validation_loss = float((model(v)-v).square().mean())
            history.append({'epoch': epoch, 'train_mse': total/len(x), 'validation_mse': validation_loss})
            if validation_loss < best:
                best, state, best_epoch = validation_loss, copy.deepcopy(model.state_dict()), epoch
            if epoch % 10 == 0: print('latent', dimension, 'epoch', epoch, 'validation', validation_loss, flush=True)
        model.load_state_dict(state); model.eval(); models[dimension] = model
        path = args.cache/f'autoencoder-{dimension}.pt'; torch.save(state, path)
        candidates.append({'model': 'autoencoder', 'dimension': dimension, 'best_epoch': best_epoch,
                           'validation_mse': best, 'history': history,
                           'parameters': sum(p.numel() for p in model.parameters()),
                           'checkpoint_sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        pca = PCA(n_components=dimension, svd_solver='full').fit(x.numpy())
        models[f'pca{dimension}'] = pca
        val_reconstruction = np.clip(pca.inverse_transform(pca.transform(v.numpy())), 0, 1)
        candidates.append({'model': 'PCA', 'dimension': dimension,
                           'validation_mse': float(errors(v.numpy(), val_reconstruction).mean())})
    # All checkpoints/configurations are frozen before this first test evaluation.
    test_predictions = {}
    for entry in candidates:
        d = entry['dimension']
        if entry['model'] == 'autoencoder':
            with torch.no_grad(): prediction = models[d](t).numpy()
        else:
            pca = models[f'pca{d}']; prediction = np.clip(pca.inverse_transform(pca.transform(t.numpy())), 0, 1)
        per_image = errors(t.numpy(), prediction)
        entry['test_mse'] = float(per_image.mean())
        entry['test_image_mse_standard_error'] = float(per_image.std(ddof=1)/np.sqrt(len(per_image)))
        entry['test_mse_by_class'] = {str(c): float(per_image[test_labels==c].mean()) for c in range(10)}
        test_predictions[(entry['model'], d)] = prediction
    mean_image = x.mean(0).numpy()
    report = {'dataset': 'Fashion-MNIST', 'source_revision': REVISION, 'source_sha256': HASHES,
              'train': len(x), 'validation': len(v), 'test': len(t), 'seed': 2026,
              'maximum_epochs': args.epochs, 'batch_size': 256, 'learning_rate': .001,
              'selection': 'lowest validation reconstruction MSE; test only after freezing both dimensions',
              'mean_image_test_mse': float(errors(t.numpy(), mean_image[None].repeat(len(t),0)).mean()),
              'runs': candidates, 'seconds': time.monotonic()-began,
              'environment': {'python': platform.python_version(), 'torch': torch.__version__, 'numpy': np.__version__},
              'code_sha256': {name: hashlib.sha256((Path(__file__).parent/name).read_bytes()).hexdigest()
                              for name in ['run.py','fashion_data.py','ae_model.py']}}
    (args.output/'metrics.json').write_text(json.dumps(report, indent=2)+'\n')
    chosen = [int(np.flatnonzero(test_labels==c)[0]) for c in range(10)]
    rows = [('Original', t.numpy()), ('PCA · 2', test_predictions[('PCA',2)]),
            ('Autoencoder · 2',test_predictions[('autoencoder',2)]),
            ('PCA · 30',test_predictions[('PCA',30)]), ('Autoencoder · 30',test_predictions[('autoencoder',30)])]
    fig, axes = plt.subplots(5,10,figsize=(12,6),layout='constrained')
    for row,(label,pred) in enumerate(rows):
        for col,index in enumerate(chosen):
            axes[row,col].imshow(pred[index].reshape(28,28),cmap='gray',vmin=0,vmax=1)
            axes[row,col].set_xticks([]); axes[row,col].set_yticks([])
        axes[row,0].set_ylabel(label, fontsize=8)
    fig.suptitle('First official test image per class · no best-example selection')
    fig.savefig(args.output/'reconstructions.png',dpi=150); plt.close(fig)
    with torch.no_grad(): latent=models[2].encoder(t).numpy()
    fig, ax=plt.subplots(figsize=(7,5),layout='constrained')
    points=ax.scatter(latent[:,0],latent[:,1],c=test_labels,cmap='tab10',s=3,alpha=.4)
    fig.colorbar(points,ax=ax,ticks=range(10),label='Class, used only for coloring')
    ax.set(title='Two coordinates are not ten clean classes',xlabel='Latent coordinate 1',ylabel='Latent coordinate 2')
    fig.savefig(args.output/'latent.png',dpi=150);plt.close(fig)
    print(json.dumps([{k:v for k,v in r.items() if k not in ['history','test_mse_by_class']} for r in candidates],indent=2))

if __name__ == '__main__': main()
