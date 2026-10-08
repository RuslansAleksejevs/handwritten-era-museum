[← Chapter](README.md) · [Origin ledger](../../docs/origins.md)

# A fragment from the original

Source: learning archive / NN4science_2_linear_models.ipynb. Location: cell 24, loss normalization through return. The excerpt is preserved below; source-file fingerprints are recorded in the origin manifest.

```python
      loss /= current_batch_size

      loss += self.reg * np.sum(self.W * self.W)
      # We must take derivative of regularization too
      # Gradient computed over the batch

      dW = dW+self.reg*2*self.W # Так ведь? Добавив регуляризацию, производная по k-ому весу вырастает на lambda*d(sum(w_i^2))/dw_k=lambda*2*w_k.
      dW /= current_batch_size
          
      return loss, dW
```

The selection preserves the source text except newline normalization. It is historical evidence, not executable modern code. Notebook cell numbers are zero-based. Source and excerpt fingerprints are recorded in [the manifest](../../docs/source-manifest.json). Commit dates describe uploads, not the exact moment of authorship.
