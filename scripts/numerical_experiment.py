"""Actual benchmark timings plus an independent LAPACK-backed NumPy oracle."""
import csv
import io
import json
import platform
import subprocess
import tempfile
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

root=Path(__file__).resolve().parents[1]
out=root/"projects/numerical-methods/results";out.mkdir(exist_ok=True)
subprocess.run(["make","build/benchmark","build/inverse"],cwd=root,check=True)
r=subprocess.run([str(root/"build/benchmark")],text=True,capture_output=True,check=True,timeout=90)
(out/"benchmark.csv").write_text(r.stdout)
rows=list(csv.DictReader(io.StringIO(r.stdout)))
rng=np.random.default_rng(2019);checks=0;largest=0.
with tempfile.TemporaryDirectory() as d:
    file=Path(d)/"matrix.txt"
    for n in (1,2,3,8,20):
        for _ in range(5):
            a=rng.normal(size=(n,n))+n*np.eye(n)
            file.write_text(str(n)+"\n"+"\n".join(" ".join(map(repr,row.tolist())) for row in a)+"\n")
            reference=np.linalg.inv(a)
            for threads in (1,2,4):
                result=subprocess.run([str(root/"build/inverse"),str(file),str(threads)],
                                      text=True,capture_output=True,check=True,timeout=5)
                got=np.loadtxt(io.StringIO(result.stdout)).reshape(n,n)
                np.testing.assert_allclose(got,reference,rtol=1e-10,atol=1e-12)
                largest=max(largest,float(np.max(np.abs(got-reference))));checks+=1
report={"oracle":"numpy.linalg.inv / LAPACK", "comparisons":checks,"max_absolute_difference":largest,
        "machine_architecture":platform.machine(),"python":platform.python_version(),"numpy":np.__version__,
        "timing_scope":"complete inverse call including worker setup and residual computation; no file I/O",
        "note":"three repeats; small matrices; not a claim of general parallel speedup"}
(out/"verification.json").write_text(json.dumps(report,indent=2)+"\n")
fig,ax=plt.subplots(figsize=(8,3.5),layout="constrained",facecolor="#f5f1e8")
for threads,color in [(1,"#243c45"),(2,"#a64d35"),(4,"#b9a77d")]:
    medians=[np.median([float(r["seconds"]) for r in rows if int(r["size"])==n and int(r["threads"])==threads]) for n in [16,64,128]]
    ax.plot([16,64,128],medians,"o-",label=f"{threads} worker(s)",color=color)
ax.set(xlabel="Matrix dimension",ylabel="Seconds · median of 3",title="Measure before claiming a speedup",yscale="log")
ax.legend(frameon=False);fig.savefig(out/"benchmark.png",dpi=170);plt.close(fig)
print(json.dumps(report,indent=2))
