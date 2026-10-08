"""A new temporal-lag study on UCI Bike Sharing; not a Delivery Club rerun."""
import argparse
import csv
from datetime import date,timedelta
import hashlib
import io
import json
from pathlib import Path
import platform
import sys
import zipfile
import numpy as np
import sklearn
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'autoencoder'))
from fashion_data import fetch
from forecast import rolling,metrics

URL='https://archive.ics.uci.edu/static/public/275/bike+sharing+dataset.zip'
SHA='b70182d0d0508e9abbb79306ce5c0cec34869000f8220175ac83d11dbe845401'


def load_daily(cache,download=False):
    path=fetch(URL,Path(cache)/'bike-sharing.zip',SHA,download,1_000_000)
    with zipfile.ZipFile(path) as archive:
        rows=list(csv.DictReader(io.StringIO(archive.read('day.csv').decode())))
    dates=[date.fromisoformat(row['dteday']) for row in rows]
    if len(rows)!=731 or any(b-a!=timedelta(days=1) for a,b in zip(dates,dates[1:])):
        raise ValueError('expected 731 ordered consecutive daily observations')
    counts=np.array([float(row['cnt']) for row in rows])
    if not np.isfinite(counts).all() or np.any(counts<0):raise ValueError('invalid counts')
    return dates,counts


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache',type=Path,default=Path('/tmp/museum-ml-extensions'))
    parser.add_argument('--output',type=Path,default=Path(__file__).parent/'results')
    parser.add_argument('--download',action='store_true');args=parser.parse_args()
    dates,counts=load_daily(args.cache,args.download)
    start=dates.index(date(2012,7,1));cutoff=dates.index(date(2012,9,1))
    candidates=[]
    for alpha in [.1,1.,10.,100.,1000.]:
        rows=rolling(counts,dates,start,cutoff,alpha)
        candidates.append({'alpha':alpha,'validation':metrics(rows)})
    selected=min(candidates,key=lambda c:c['validation']['ridge']['mae'])
    validation=rolling(counts,dates,start,cutoff,selected['alpha'])
    # Freeze alpha before forecasting the untouched September-December holdout.
    # Earlier holdout observations become available only at later weekly origins.
    test=rolling(counts,dates,cutoff,len(dates),selected['alpha'])
    report={'dataset':'UCI Bike Sharing, daily Capital Bikeshare demand, 2011-2012',
        'source':URL,'sha256':SHA,'license':'CC BY 4.0','doi':'10.24432/C5W894',
        'original_scope':'Delivery Club training CSV absent; this is a separate modern substitute-data experiment',
        'initial_history':['2011-01-01','2012-06-30'],'validation':['2012-07-01','2012-08-31'],
        'test':['2012-09-01','2012-12-31'],'validation_days':len(validation),'test_days':len(test),
        'forecast':'refit from all observed history at each weekly origin; predict up to seven days without consuming within-horizon targets',
        'features':'demand lags 7/14/21/28, linear calendar trend, yearly sin/cos and weekday indicators; no observed future weather or demand components',
        'selected_alpha':selected['alpha'],'candidates':candidates,'validation_metrics':metrics(validation),'test_metrics':metrics(test),
        'test_by_horizon':{str(day):metrics([r for r in test if r['horizon_day']==day]) for day in range(1,8)},
        'environment':{'python':platform.python_version(),'numpy':np.__version__,'sklearn':sklearn.__version__},
        'code_sha256':{n:hashlib.sha256((Path(__file__).parent/n).read_bytes()).hexdigest() for n in ['run.py','forecast.py']}}
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
    (args.output/'predictions.json').write_text(json.dumps({'validation':validation,'test':test},indent=2)+'\n')
    fig,axes=plt.subplots(2,1,figsize=(12,7),layout='constrained')
    axes[0].plot(dates,counts,color='#8b7c51',lw=1);axes[0].axvspan(dates[start],dates[cutoff],alpha=.2,color='#b9a77d',label='Validation')
    axes[0].axvspan(dates[cutoff],dates[-1],alpha=.1,color='#243c45',label='Test');axes[0].set(title='Calendar order matters',ylabel='Daily rentals');axes[0].legend()
    ts=[date.fromisoformat(r['date']) for r in test]
    for name,color in [('actual','#243c45'),('ridge','#a64d35'),('four_week_mean','#b9a77d')]:axes[1].plot(ts,[r[name] for r in test],label=name,color=color,lw=1.2)
    axes[1].set(title='Weekly forecasts · labels enter later training only after observation',ylabel='Daily rentals');axes[1].legend()
    fig.savefig(args.output/'forecast.png',dpi=150);plt.close(fig)
    print(json.dumps({'selected_alpha':selected['alpha'],'test':report['test_metrics']},indent=2))

if __name__=='__main__':main()
