[← Machine learning](README.md)

# Recorded runs and later edits

The saved results in **segmentation, transfer, autoencoder and forecasting** were
produced before the readability refactor of 7 October 2026. Their `metrics.json`
files still contain the SHA-256 fingerprints of the code that actually ran.
They have not been replaced with hashes of a later implementation.

The matching source is included in [recorded-source](recorded-source/README.md),
with [file fingerprints](recorded-source/manifest.json). These are the original
2026 experiment implementations; no private repository or old Git history is needed.

The current scripts separate argument parsing, training/selection, test scoring
and plotting. The six supporting data/model modules received formatting only;
their parsed Python syntax trees were checked against the recorded version and
were identical. The experiment settings and selection rules were retained.

## What was checked after the refactor

Both versions ran on the same fixed synthetic fixtures in the existing local
environment, with no downloads:

- **Segmentation:** 18 small images, two epochs for each actual network.
- **Autoencoders:** 50 training, 30 validation and 20 test images, two epochs for
  each actual network, plus both PCA fits.
- **Transfer:** the 3,000/1,000/2,000 label budgets and actual scaler/ridge selection
  pipeline, with small deterministic stand-in feature extractors to keep the
  check bounded. This did not repeat pretrained SqueezeNet extraction.
- **Forecasting:** a synthetic 731-day series through the complete weekly
  selection and evaluation pipeline.

Numeric reports, splits, selected examples and forecast rows matched exactly;
all generated plots matched pixel for pixel. Every saved segmentation and
autoencoder parameter tensor matched exactly. Elapsed time, source-code hashes
and checkpoint archive hashes were excluded from report comparison; the actual
checkpoint tensors were compared instead. The normal repository checks also
passed.

The saved full-data metrics, splits, examples and figures remain byte-for-byte
unchanged. The fixture check above was followed by a full-data rerun on
8 October 2026 in a separate source copy and newly installed Python environment
on macOS arm64. It repeated all four current scripts, including pretrained
SqueezeNet extraction. Metrics, splits, examples and forecast rows matched
after excluding elapsed time and the intentionally different source hashes;
all figure pixels matched. The four segmentation/autoencoder checkpoint files
also matched byte for byte. [Fresh-run scope and limits](../../docs/reproduce.md).
