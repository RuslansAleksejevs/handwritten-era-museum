"""Synthetic fixtures for binary decoding, split isolation and scoring."""
import sys
from pathlib import Path
import unittest
import numpy as np
import torch
from torch import nn
from torch.utils.data import DataLoader, TensorDataset
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'projects/machine-learning/cnn'))
from cifar_data import decode_batch, stratified_split, channel_statistics
from cnn_models import make_model, evaluate


class CNNTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(2)

    def test_binary_channel_order_and_invalid_rows(self):
        record = bytes([7]) + bytes([10]) * 1024 + bytes([20]) * 1024 + bytes([30]) * 1024
        images, labels = decode_batch(record, count=1)
        self.assertEqual(labels.tolist(), [7])
        self.assertEqual(images[0, :, 0, 0].tolist(), [10, 20, 30])
        with self.assertRaises(ValueError): decode_batch(record[:-1], count=1)
        with self.assertRaises(ValueError): decode_batch(bytes([10]) + record[1:], count=1)

    def test_stratification_is_disjoint_and_repeatable(self):
        labels = np.repeat(np.arange(3), 20)
        train, val = stratified_split(labels, 4)
        self.assertFalse(set(train) & set(val))
        self.assertEqual(sorted(np.r_[train, val]), list(range(60)))
        np.testing.assert_equal(np.bincount(labels[val]), [4, 4, 4])
        np.testing.assert_equal(stratified_split(labels, 4)[0], train)
        with self.assertRaises(ValueError): stratified_split(labels, 20)

    def test_validation_pixels_cannot_change_training_scaling(self):
        images = np.zeros((4, 3, 32, 32), dtype=np.uint8)
        images[1] = 100
        mean, std = channel_statistics(images, [0, 1])
        images[2:] = 255
        changed = channel_statistics(images, [0, 1])
        np.testing.assert_equal(mean, changed[0])
        np.testing.assert_equal(std, changed[1])
        np.testing.assert_allclose(mean, np.full(3, 50 / 255))

    def test_scoring_weights_last_batch_and_restores_training_mode(self):
        class Fixture(nn.Module):
            def forward(self, x):
                # Different losses on the final partial batch expose batch-mean averaging.
                value = x[:, 0, 0, 0] * 10
                return torch.stack([value, -value], dim=1)
        x = torch.zeros((5, 3, 32, 32), dtype=torch.uint8)
        x[-1] = 255
        y = torch.tensor([0, 1, 0, 1, 1])
        loader = DataLoader(TensorDataset(x, y), batch_size=3)
        model = Fixture().train()
        score = evaluate(model, loader, torch.zeros(3), torch.ones(3))
        expected = torch.nn.functional.cross_entropy(model(x.float() / 255), y).item()
        self.assertAlmostEqual(score['nll'], expected, places=6)
        self.assertTrue(model.training)
        self.assertEqual(score['n'], 5)

    def test_models_take_rgb_and_gradient_reaches_every_layer(self):
        for name in ['linear', 'historical', 'compact']:
            torch.manual_seed(19)
            model = make_model(name)
            logits = model(torch.randn(4, 3, 32, 32))
            self.assertEqual(tuple(logits.shape), (4, 10))
            torch.nn.functional.cross_entropy(logits, torch.arange(4)).backward()
            self.assertTrue(all(p.grad is not None and torch.isfinite(p.grad).all() for p in model.parameters()))

    def test_compact_network_can_fit_a_small_synthetic_pattern(self):
        torch.manual_seed(7)
        x = torch.randn(12, 3, 32, 32) * .05
        y = torch.arange(12) % 2
        x[y == 0, :, :, :16] += 1
        x[y == 1, :, :, 16:] += 1
        model = make_model('compact')
        optimizer = torch.optim.Adam(model.parameters(), lr=.01)
        for _ in range(20):
            optimizer.zero_grad()
            loss = torch.nn.functional.cross_entropy(model(x), y)
            loss.backward()
            optimizer.step()
        self.assertGreaterEqual((model(x).argmax(1) == y).float().mean().item(), .9)


if __name__ == '__main__':
    unittest.main()
