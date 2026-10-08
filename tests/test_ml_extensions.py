"""Small offline invariants for the four ML museum extensions; no dataset download."""

from datetime import date, timedelta
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[1] / "projects/machine-learning"
for folder in ["autoencoder", "transfer", "segmentation", "forecasting"]:
    sys.path.insert(0, str(ROOT / folder))
from fashion_data import read_idx, selected_split, fetch
from ae_model import Autoencoder, errors
from squeeze_features import frozen_model, preprocess
from car_data import source_group, assign_groups
from segment_model import SmallUNet, PixelClassifier, confusion, scores
from forecast import features, forecast_origin, rolling, metrics


class ExtensionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(2)

    def test_idx_rejects_bad_shape_and_truncation(self):
        raw = struct.pack(">IIII", 2051, 2, 28, 28) + bytes(2 * 784)
        self.assertEqual(read_idx(raw).shape, (2, 784))
        np.testing.assert_array_equal(
            read_idx(struct.pack(">II", 2049, 3) + b"\x01\x02\x03"), [1, 2, 3]
        )
        for broken in [
            raw[:-1],
            b"",
            struct.pack(">IIII", 2051, 2, 14, 56) + bytes(2 * 784),
        ]:
            with self.assertRaises(ValueError):
                read_idx(broken)

    def test_cache_fingerprint_rejects_corruption(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture"
            path.write_bytes(b"broken")
            with self.assertRaisesRegex(ValueError, "checksum"):
                fetch("unused", path, "0" * 64)

    def test_fashion_selection_disjoint_and_exact_test_images_excluded(self):
        images = np.array(
            [[label, example] for label in range(10) for example in range(8)],
            dtype=np.uint8,
        )
        labels = np.repeat(np.arange(10), 8)
        heldout = images[::8]
        tr, va, _ = selected_split(
            images, labels, heldout, train_per_class=2, val_per_class=2
        )
        self.assertFalse(set(tr) & set(va))
        self.assertFalse(
            {row.tobytes() for row in images[np.r_[tr, va]]}
            & {r.tobytes() for r in heldout}
        )
        self.assertEqual(np.bincount(labels[tr]).tolist(), [2] * 10)

    def test_autoencoder_bottleneck_output_bounds_and_gradients(self):
        for dimension in [2, 30]:
            model = Autoencoder(dimension)
            x = torch.rand(3, 784)
            output = model(x)
            self.assertEqual(model.encoder(x).shape, (3, dimension))
            self.assertEqual(output.shape, x.shape)
            self.assertTrue(torch.all((output >= 0) & (output <= 1)))
            (output - x).square().mean().backward()
            self.assertTrue(
                all(
                    p.grad is not None and torch.isfinite(p.grad).all()
                    for p in model.parameters()
                )
            )
        np.testing.assert_allclose(errors(np.zeros((2, 3)), np.ones((2, 3))), [1, 1])
        with self.assertRaises(ValueError):
            errors(np.zeros((2, 3)), np.ones((2, 4)))

    def test_preprocessing_and_frozen_feature_shape(self):
        x = np.zeros((2, 3, 32, 32), dtype=np.uint8)
        transformed = preprocess(x)
        self.assertEqual(transformed.shape, (2, 3, 224, 224))
        np.testing.assert_allclose(
            transformed[0, :, 0, 0].numpy(),
            -np.array([0.485, 0.456, 0.406]) / np.array([0.229, 0.224, 0.225]),
            rtol=1e-6,
        )
        model = frozen_model("/unused", pretrained=False)
        self.assertFalse(model.training)
        self.assertTrue(all(not p.requires_grad for p in model.parameters()))
        self.assertEqual(model(transformed).shape, (2, 512))

    def test_capture_groups_and_exact_duplicates_never_cross_partitions(self):
        names = [
            "IMG_20201123_123456.png",
            "IMG_20201123_130000.png",
            "im2.png",
            "im9.png",
            "a.png",
            "b.png",
            "c.png",
            "d.png",
        ]
        hashes = ["one", "two", "three", "four", "one", "six", "seven", "eight"]
        split, family = assign_groups(names, hashes)
        for i, j in [(0, 1), (0, 4), (2, 3)]:
            self.assertEqual(split[i], split[j])
            self.assertEqual(family[i], family[j])
        self.assertEqual(set(split), {"train", "validation", "test"})
        self.assertEqual(source_group(names[0]), source_group(names[1]))

    def test_segmentation_shapes_and_finite_gradients(self):
        for constructor in [SmallUNet, PixelClassifier]:
            model = constructor()
            x = torch.rand(2, 3, 31, 35)
            prediction = model(x)
            self.assertEqual(prediction.shape, (2, 5, 31, 35))
            torch.nn.functional.cross_entropy(
                prediction, torch.zeros((2, 31, 35), dtype=torch.long)
            ).backward()
            self.assertTrue(
                all(
                    p.grad is not None and torch.isfinite(p.grad).all()
                    for p in model.parameters()
                )
            )

    def test_iou_uses_pooled_confusion_not_average_batch_ratios(self):
        target = np.array([0, 0, 1, 1])
        prediction = np.array([0, 1, 1, 1])
        result = scores(confusion(target, prediction))
        self.assertAlmostEqual(result["mean_iou"], (0.5 + 2 / 3) / 2)
        self.assertEqual(result["per_class_iou"][2:], [None, None, None])
        np.testing.assert_array_equal(
            confusion(target, prediction),
            confusion(target[:1], prediction[:1])
            + confusion(target[1:], prediction[1:]),
        )
        with self.assertRaises(ValueError):
            confusion(target, np.array([0, 1, 5, 1]))

    def test_seven_day_features_and_predictions_ignore_future_targets(self):
        dates = [date(2020, 1, 1) + timedelta(days=i) for i in range(120)]
        counts = 100 + np.arange(120) + np.sin(np.arange(120)) * 10
        changed = counts.copy()
        changed[90:] = 1_000_000
        a = forecast_origin(counts, dates, 90)
        b = forecast_origin(changed, dates, 90)
        for original, modified in zip(a, b):
            np.testing.assert_allclose(original, modified)
        with self.assertRaises(ValueError):
            forecast_origin(counts, dates, 90, horizon=8)
        with self.assertRaisesRegex(ValueError, "crosses"):
            features(counts, dates, [97], origin=90)

    def test_rolling_forecast_covers_each_day_once_and_seasonal_oracle(self):
        dates = [date(2020, 1, 1) + timedelta(days=i) for i in range(110)]
        counts = (np.arange(110) % 7 + 1) * 100.0
        rows = rolling(counts, dates, 90, 110, alpha=1)
        self.assertEqual(
            [r["date"] for r in rows], [d.isoformat() for d in dates[90:110]]
        )
        self.assertEqual(len({r["date"] for r in rows}), 20)
        self.assertEqual(metrics(rows)["last_week"]["mae"], 0.0)
        self.assertEqual(metrics(rows)["four_week_mean"]["mae"], 0.0)
