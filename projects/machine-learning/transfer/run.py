"""A frozen ImageNet representation versus random features on a small CIFAR-10 budget."""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import sys
import time
import numpy as np
import torch
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import RidgeClassifier
from sklearn.metrics import confusion_matrix
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "cnn"))
from cifar_data import archive_path, load_cifar, stratified_split, CLASSES
from squeeze_features import frozen_model, features, WEIGHT_SHA


def sample_classes(labels, indices, per_class, seed):
    rng = np.random.default_rng(seed)
    selected = []
    for label in range(10):
        options = indices[labels[indices] == label]
        if len(options) < per_class:
            raise ValueError("insufficient class examples")
        selected.extend(rng.permutation(options)[:per_class].tolist())
    return np.sort(selected)


def parse_args():
    """Read cache locations and the output directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=Path("/tmp/museum-ml-extensions"))
    parser.add_argument("--cifar-cache", type=Path, default=Path("/tmp/museum-cifar"))
    parser.add_argument(
        "--output", type=Path, default=Path(__file__).parent / "results"
    )
    parser.add_argument("--download", action="store_true")
    args = parser.parse_args()
    return args


def extract_representations(all_images, cache, download):
    """Build pretrained, random and raw-pixel features without using target labels."""
    names = ["ImageNet features", "Random frozen features", "Raw pixels, 8 x 8"]
    feature_sets = {}
    network_parameters = None
    for name, pretrained in zip(names[:2], [True, False]):
        model = frozen_model(cache, pretrained, download)
        network_parameters = sum(p.numel() for p in model.parameters())
        feature_sets[name] = features(model, all_images)
    feature_sets[names[2]] = (
        all_images.reshape(-1, 3, 8, 4, 8, 4)
        .mean(axis=(3, 5))
        .reshape(len(all_images), -1)
        / 255
    )
    return names, feature_sets, network_parameters


def select_classifiers(feature_sets, train_labels, validation_labels):
    """Fit scaling on training features; select ridge alpha on validation accuracy."""
    train_stop = len(train_labels)
    validation_stop = train_stop + len(validation_labels)
    runs = []
    selected = {}
    for name, values in feature_sets.items():
        trials = []
        models = []
        for alpha in [0.1, 1.0, 10.0, 100.0]:
            model = make_pipeline(StandardScaler(), RidgeClassifier(alpha=alpha)).fit(
                values[:train_stop], train_labels
            )
            score = float(
                np.mean(
                    model.predict(values[train_stop:validation_stop])
                    == validation_labels
                )
            )
            trials.append({"alpha": alpha, "validation_accuracy": score})
            models.append(model)
        best = max(range(len(trials)), key=lambda i: trials[i]["validation_accuracy"])
        selected[name] = models[best]
        runs.append(
            {
                "model": name,
                "features": values.shape[1],
                "selected_alpha": trials[best]["alpha"],
                "validation_accuracy": trials[best]["validation_accuracy"],
                "candidates": trials,
            }
        )
    return runs, selected


def evaluate_classifiers(runs, selected, feature_sets, test_start, test_labels):
    """Score test labels only after all three classifiers have been selected."""
    # The label budgets, representation, scaling and classifier are frozen before test scoring.
    prediction_sets = {}
    for run in runs:
        name = run["model"]
        prediction = selected[name].predict(feature_sets[name][test_start:])
        prediction_sets[name] = prediction
        run["test_accuracy"] = float(np.mean(prediction == test_labels))
        run["confusion_matrix"] = confusion_matrix(
            test_labels, prediction, labels=range(10)
        ).tolist()
        run["test_correct"] = int(np.sum(prediction == test_labels))
    return prediction_sets


def plot_results(
    output, names, runs, test_images, test_labels, test_indices, prediction_sets
):
    """Compare accuracy and show the first pretrained-feature error in each class."""
    fig, ax = plt.subplots(figsize=(8, 4), layout="constrained")
    ax.barh(
        names,
        [r["test_accuracy"] for r in runs],
        color=["#243c45", "#b9a77d", "#a64d35"],
    )
    ax.set(
        xlim=(0, 1),
        xlabel="Test accuracy · same 3,000 target-domain training labels",
        title="What does a frozen representation bring?",
    )
    fig.savefig(output / "comparison.png", dpi=160)
    plt.close(fig)
    fig, axes = plt.subplots(2, 5, figsize=(10, 5), layout="constrained")
    # First error in each true class, not hand-picked successes.
    shown = []
    for label, ax in enumerate(axes.flat):
        candidates = np.flatnonzero(
            (test_labels == label) & (prediction_sets[names[0]] != label)
        )
        if len(candidates):
            j = int(candidates[0])
            ax.imshow(test_images[j].transpose(1, 2, 0))
            ax.set_title(
                f"{CLASSES[label]} → {CLASSES[prediction_sets[names[0]][j]]}",
                fontsize=9,
            )
            shown.append(
                {
                    "official_test_index": int(test_indices[j]),
                    "true": label,
                    "predicted": int(prediction_sets[names[0]][j]),
                }
            )
        ax.axis("off")
    fig.suptitle("First pretrained-feature error per class in the selected test set")
    fig.savefig(output / "mistakes.png", dpi=150)
    plt.close(fig)
    (output / "examples.json").write_text(json.dumps(shown, indent=2) + "\n")


def main():
    args = parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    torch.set_num_threads(2)
    torch.use_deterministic_algorithms(True)
    path = archive_path(args.cifar_cache, args.download)
    images, labels, tx, ty = load_cifar(path)
    tr, va = stratified_split(labels)
    train = sample_classes(labels, tr, 300, 2026)
    validation = sample_classes(labels, va, 100, 2026)
    test = sample_classes(ty, np.arange(len(ty)), 200, 2026)
    split = {
        "train_indices": train.tolist(),
        "validation_indices": validation.tolist(),
        "test_indices": test.tolist(),
        "test_index_space": "official CIFAR-10 test partition",
        "base_split_seed": 2019,
        "subset_seed": 2026,
    }
    (args.output / "split.json").write_text(json.dumps(split, indent=2) + "\n")
    all_images = np.concatenate([images[train], images[validation], tx[test]])
    began = time.monotonic()
    names, feature_sets, network_parameters = extract_representations(
        all_images, args.cache, args.download
    )
    runs, selected = select_classifiers(feature_sets, labels[train], labels[validation])
    prediction_sets = evaluate_classifiers(
        runs, selected, feature_sets, len(train) + len(validation), ty[test]
    )
    report = {
        "dataset": "CIFAR-10 binary edition",
        "dataset_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "train": 3000,
        "validation": 1000,
        "test": 2000,
        "seed": 2026,
        "pretraining": "ImageNet-1K; extra labeled external data, not a matched total-data budget",
        "weights_sha256": WEIGHT_SHA,
        "feature_network_parameters": network_parameters,
        "selection": "maximize validation accuracy among ridge alpha .1,1,10,100; ties choose first; no refit on validation",
        "normalization": "ImageNet fixed normalization for both CNNs; downstream scaler fit on 3000 training feature vectors only",
        "runs": runs,
        "seconds": time.monotonic() - began,
        "environment": {
            "python": platform.python_version(),
            "torch": torch.__version__,
            "numpy": np.__version__,
        },
        "code_sha256": {
            n: hashlib.sha256((Path(__file__).parent / n).read_bytes()).hexdigest()
            for n in ["run.py", "squeeze_features.py"]
        },
    }
    (args.output / "metrics.json").write_text(json.dumps(report, indent=2) + "\n")
    plot_results(args.output, names, runs, tx[test], ty[test], test, prediction_sets)
    print(
        json.dumps(
            [{k: v for k, v in r.items() if k != "confusion_matrix"} for r in runs],
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
