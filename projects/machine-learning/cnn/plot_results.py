"""Export the fixed first seed's errors and aggregate curves, without selecting a winning seed."""
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from cifar_data import CLASSES

COLORS = {'linear': '#aa6b45', 'historical': '#91846a', 'compact': '#244a58'}
LABELS = {'linear': 'Linear baseline', 'historical': 'Historical CNN', 'compact': 'Modern compact CNN'}


def render(report, predictions, images, output):
    output = Path(output)
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10, 'axes.spines.top': False,
                         'axes.spines.right': False, 'figure.facecolor': '#f5f1e8'})
    fig, axes = plt.subplots(1, 2, figsize=(11, 4), layout='constrained')
    for index, (name, color) in enumerate(COLORS.items()):
        runs = [r for r in report['runs'] if r['model'] == name]
        vals = np.array([[e['validation']['accuracy'] for e in r['history']] for r in runs])
        epochs = np.arange(1, vals.shape[1] + 1)
        axes[0].plot(epochs, vals.mean(axis=0) * 100, color=color, label=LABELS[name])
        axes[0].fill_between(epochs, vals.min(axis=0) * 100, vals.max(axis=0) * 100, color=color, alpha=.13)
        scores = np.array([r['test']['accuracy'] for r in runs]) * 100
        axes[1].bar(index, scores.mean(), color=color, alpha=.85)
        axes[1].scatter(index + np.linspace(-.09, .09, len(scores)), scores, color='#171f25', s=20)
        axes[1].text(index, scores.max() + 1, f'{scores.mean():.1f}%', ha='center')
    axes[0].set(xlabel='Epoch', ylabel='Validation accuracy (%)', title='Same split and training budget')
    axes[0].legend(frameon=False, fontsize=9)
    axes[1].set(xticks=range(3), xticklabels=['Linear', 'Historical CNN', 'Compact CNN'],
                ylabel='Test accuracy (%)', ylim=(0, 100), title='All seeds · dots show individual runs')
    fig.savefig(output / 'comparison.png', dpi=170)
    plt.close(fig)
    seed = report['config']['seeds'][0]
    selected = next(r for r in predictions if r['model'] == 'compact' and r['seed'] == seed)
    truth = np.asarray(selected['targets'])
    guessed = np.asarray(selected['predictions'])
    confidence = np.asarray(selected['confidence'])
    audits = []
    for run in predictions:
        matrix = np.zeros((10, 10), dtype=int)
        np.add.at(matrix, (run['targets'], run['predictions']), 1)
        audits.append({'model': run['model'], 'seed': run['seed'], 'confusion_counts': matrix.tolist()})
    matrix = np.array(next(r['confusion_counts'] for r in audits if r['model'] == 'compact' and r['seed'] == seed))
    fig, ax = plt.subplots(figsize=(7.5, 6.5), layout='constrained')
    im = ax.imshow(matrix / matrix.sum(axis=1, keepdims=True), cmap='Blues', vmin=0, vmax=1)
    for i in range(10):
        for j in range(10):
            if matrix[i, j]:
                ax.text(j, i, str(matrix[i, j]), ha='center', va='center', fontsize=7,
                        color='white' if matrix[i, j] > matrix[i].sum() * .55 else '#172e3b')
    ax.set(xticks=range(10), yticks=range(10), xticklabels=CLASSES, yticklabels=CLASSES,
           xlabel='Predicted class', ylabel='True class', title=f'Compact CNN · fixed seed {seed}\nCounts; colour is the fraction of each true class')
    plt.setp(ax.get_xticklabels(), rotation=45, ha='right')
    fig.colorbar(im, ax=ax, shrink=.75)
    fig.savefig(output / 'confusion.png', dpi=160)
    plt.close(fig)
    wrong = np.flatnonzero(guessed != truth)
    chosen = sorted(wrong, key=lambda i: (-confidence[i], i))[:12]
    fig, axes = plt.subplots(3, 4, figsize=(9, 7), layout='constrained')
    examples = []
    for ax in axes.flat:
        ax.axis('off')
    for ax, i in zip(axes.flat, chosen):
        ax.imshow(images[i].transpose(1, 2, 0), interpolation='nearest')
        ax.set_title(f'True: {CLASSES[truth[i]]}\nPredicted: {CLASSES[guessed[i]]} · {confidence[i]:.0%}', fontsize=9)
        examples.append({'test_index': int(i), 'true': CLASSES[truth[i]], 'predicted': CLASSES[guessed[i]],
                         'softmax_confidence': float(confidence[i])})
    fig.suptitle(f'Confident mistakes · compact CNN, fixed seed {seed}\n12 highest-confidence errors; confidence is not a calibrated probability', fontsize=12)
    fig.savefig(output / 'mistakes.png', dpi=150)
    plt.close(fig)
    (output / 'error-analysis.json').write_text(json.dumps({'classes': CLASSES, 'runs': audits,
        'illustration_seed': seed, 'example_rule': 'first configured seed; highest-confidence wrong predictions, ties by test index',
        'examples': examples}, indent=2) + '\n')
