[← Chapter](README.md) · [Origin ledger](../../docs/origins.md)

# A fragment from the original

**For complete historical functions and a full notebook solution, open [the unchanged originals](../../originals/README.md).**

Source: algorithms archive / contests/biweekly99.ipynb. Location: cell 24, restoring state after dfs2. The excerpt is preserved below; source-file fingerprints are recorded in the origin manifest.

```python
            #сначала забыл следующий блок, а он очень важный. В основном коде программы dfs2 мы будем вызывать для всех соседей той вершины, к которой мы применили dfs1, поэтому нужно, чтобы в итоге наше множество было таким, как в самом начале, это очень важно. Вернем его в предыдущее состояние.
            if(indicator1):
                edges_set.add((prev,v))
            if(indicator2):
                edges_set.remove((v,prev))
```

The selection preserves the source text except newline normalization. It is historical evidence, not executable modern code. Notebook cell numbers are zero-based. Source and excerpt fingerprints are recorded in [the manifest](../../docs/source-manifest.json). Commit dates describe uploads, not the exact moment of authorship.
