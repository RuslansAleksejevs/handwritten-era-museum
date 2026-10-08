"""Offline, fixed-protocol experiment. No download and no test-set tuning."""
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from sklearn.datasets import load_digits
from sklearn.decomposition import PCA
from sklearn.model_selection import StratifiedKFold, cross_val_score, train_test_split
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.svm import LinearSVC
from linear import finite_difference_error, fit_linear


def main():
    out = Path(__file__).parent / "results"
    out.mkdir(exist_ok=True)
    x, y = load_digits(return_X_y=True)
    train, test = train_test_split(np.arange(len(x)), test_size=.25, random_state=2019, stratify=y)
    scaler = StandardScaler().fit(x[train])
    a = np.column_stack((scaler.transform(x[train]), np.ones(len(train))))
    b = np.column_stack((scaler.transform(x[test]), np.ones(len(test))))
    weights, losses = fit_linear(a, y[train], classes=10)
    prediction = (b @ weights).argmax(axis=1)
    folds = StratifiedKFold(4, shuffle=True, random_state=2019)
    cv = {}
    for components in (None, 16, 32):
        steps = [StandardScaler()]
        if components is not None:
            steps.append(PCA(n_components=components, svd_solver="full"))
        steps.append(LinearSVC(C=.1, max_iter=10000, random_state=2019))
        # Every scaler and PCA is fit separately inside each training fold.
        scores = cross_val_score(make_pipeline(*steps), x[train], y[train], cv=folds)
        cv[str(components)] = scores.tolist()
    rng = np.random.default_rng(2019)
    gradient_error = finite_difference_error(rng.normal(size=(7, 4)), np.arange(7) % 3,
                                            rng.normal(scale=.01, size=(4, 3)), .2)
    report = {"seed": 2019, "dataset": "sklearn digits (8x8)", "train_examples": len(train),
              "test_examples": len(test), "test_accuracy": float(np.mean(prediction == y[test])),
              "final_train_loss": losses[-1], "finite_difference_max_error": gradient_error,
              "training_only_cv_accuracy": cv,
              "scope": "Modern experiment; not a rerun of historical CIFAR/MNIST course results."}
    (out / "metrics.json").write_text(json.dumps(report, indent=2) + "\n")
    fig, axes = plt.subplots(1, 2, figsize=(10, 3.6), layout="constrained")
    fig.patch.set_facecolor("#f5f1e8")
    axes[0].plot(losses, color="#a64d35"); axes[0].set(xlabel="Gradient step", ylabel="Objective", title="A loss with its own gradient")
    # The zoomed accuracy axis uses markers: bar lengths from .8 would exaggerate the gaps.
    for i, (scores, color) in enumerate(zip(cv.values(), ["#243c45", "#a64d35", "#b9a77d"])):
        axes[1].scatter([i] * len(scores), scores, color=color, alpha=.45, s=18)
        axes[1].errorbar(i, np.mean(scores), yerr=np.std(scores), fmt="o", color=color, capsize=4, markersize=7)
    axes[1].set_xticks(range(3), ["64 features", "PCA 16", "PCA 32"])
    axes[1].set(xlim=(-.5,2.5), ylim=(.8,1), ylabel="Training-fold CV accuracy",
                title="Preprocessing stays inside the fold\nfour fold scores, mean ± SD")
    fig.savefig(out / "experiment.png", dpi=170); plt.close(fig)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
