[← Chapter](README.md) · [Origin ledger](../../docs/origins.md)

# A few lines from the originals

**For complete historical functions and a full notebook solution, open [the unchanged originals](../../originals/README.md).**

Source: Numerical archive / Jordan/solver.h. Location: lines 13–18. The excerpt is preserved below; source-file fingerprints are recorded in the origin manifest.

```cpp

    // первый шаг ручками. ГЛАВНЫЙ ПО СТРОКЕ
    size_t ind;
    ind=A.MainElement_Row(0,0);
    E.SwapColumns(0,ind); A.SwapColumns(0,ind); //сначала делаем для E, т.к. E зависит от того, что в А, а А не зависит от того, что в E.
    E.MultColumn(0,1/A.mat[0][0]); A.MultColumn(0,1/A.mat[0][0]);
```

The selection preserves the source text except newline normalization. It is historical evidence, not executable modern code. Notebook cell numbers are zero-based. Source and excerpt fingerprints are recorded in [the manifest](../../docs/source-manifest.json). Commit dates describe uploads, not the exact moment of authorship.

## The parallel idea that survived

Source: Jordan_MPI/solver.h, lines 77–81. A short selected fragment, with its original comments:

```cpp
        for(size_t i=rank; i<n; i=i+size) // основное разделение работы между процессами здесь
        {
            if(i!=k)  // if очень важен, иначе появится нулевая строка, но богачеву в книжке пофиг
            {
                E.SubtractColumns(i,k,A.mat[i][k]);
```

The first comment marks where the work is split across processes. The following note insists on excluding the pivot column: otherwise it would subtract itself. The [modern MPI implementation](mpi/README.md) keeps that cyclic column assignment while replacing replicated matrices and repeated scalar broadcasts with explicit ownership and collective pivot selection.
