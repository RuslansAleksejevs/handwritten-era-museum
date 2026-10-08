"""Five-class car segmentation on the recovered 211-image course dataset."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import platform
import time
import numpy as np
import torch
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap
from car_data import load_cars,ARCHIVE_SHA,CLASSES,SIZE,PUBLIC_URL
from segment_model import SmallUNet,PixelClassifier,confusion,scores

@torch.no_grad()
def predict(model,x):
    model.eval()
    return np.concatenate([model(chunk).argmax(1).numpy() for chunk in x.split(16)])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache',type=Path,default=Path('/tmp/museum-ml-extensions'))
    parser.add_argument('--output',type=Path,default=Path(__file__).parent/'results')
    parser.add_argument('--download',action='store_true');parser.add_argument('--epochs',type=int,default=40)
    args=parser.parse_args()
    if args.epochs<1:parser.error('positive epochs required')
    torch.set_num_threads(2);torch.use_deterministic_algorithms(True)
    images,masks,entries=load_cars(args.cache,args.download)
    partitions={s:np.array([i for i,e in enumerate(entries) if e['split']==s]) for s in ['train','validation','test']}
    x=torch.from_numpy(images.astype(np.float32)/255);y=torch.from_numpy(masks)
    tr,va,te=[partitions[s] for s in ['train','validation','test']]
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'split.json').write_text(json.dumps({'seed':2026,'entries':entries,
        'rule':'group capture dates, im-numbered and image-numbered filename series, and exact RGB duplicates; largest groups first, allocate to lowest filled fraction of 70/15/15 targets'},indent=2)+'\n')
    counts=np.bincount(masks[tr].ravel(),minlength=5)
    weight=np.sqrt(counts.sum()/np.maximum(counts,1));weight/=weight.mean()
    criterion=torch.nn.CrossEntropyLoss(weight=torch.tensor(weight,dtype=torch.float32))
    runs=[];trained={};began=time.monotonic()
    for name,constructor in [('Color-only network',PixelClassifier),('U-Net',SmallUNet)]:
        torch.manual_seed(2026);model=constructor();optimizer=torch.optim.Adam(model.parameters(),lr=.002)
        history=[];best=-1;state=None
        for epoch in range(1,args.epochs+1):
            model.train();order=tr[torch.randperm(len(tr)).numpy()];total=0
            for start in range(0,len(order),8):
                index=order[start:start+8];a,b=x[index].clone(),y[index].clone()
                flip=torch.rand(len(index))<.5;a[flip]=a[flip].flip(-1);b[flip]=b[flip].flip(-1)
                optimizer.zero_grad();loss=criterion(model(a),b);loss.backward();optimizer.step()
                total+=float(loss.detach())*len(index)
            metric=scores(confusion(masks[va],predict(model,x[va])))
            history.append({'epoch':epoch,'train_loss':total/len(tr),'validation_mean_iou':metric['mean_iou']})
            if metric['mean_iou']>best:best,state,best_epoch=metric['mean_iou'],copy.deepcopy(model.state_dict()),epoch
            if epoch%10==0:print(name,epoch,metric['mean_iou'],flush=True)
        model.load_state_dict(state);model.eval();trained[name]=model
        checkpoint=args.cache/('segmentation-'+('unet' if name=='U-Net' else 'pixel')+'.pt');torch.save(state,checkpoint)
        runs.append({'model':name,'parameters':sum(p.numel() for p in model.parameters()),'best_epoch':best_epoch,
                     'validation_mean_iou':best,'history':history,'checkpoint_sha256':hashlib.sha256(checkpoint.read_bytes()).hexdigest()})
    predictions={}
    for run in runs:
        pred=predict(trained[run['model']],x[te]);predictions[run['model']]=pred
        run['test']=scores(confusion(masks[te],pred))
        run['test_by_image']=[{'id':entries[i]['id'],**scores(confusion(masks[i],p))} for i,p in zip(te,pred)]
    majority=np.full_like(masks[te],int(counts.argmax()))
    report={'dataset':'Recovered course car-segmentation.zip','source':PUBLIC_URL,'archive_sha256':ARCHIVE_SHA,
        'classes':CLASSES,'size':SIZE,'counts':{s:len(v) for s,v in partitions.items()},'seed':2026,
        'epochs':args.epochs,'learning_rate':.002,'batch_size':8,'class_weights_train_only':weight.tolist(),
        'augmentation':'independent horizontal flip for training images and masks together',
        'selection':'highest validation pooled mean IoU; test scored after both checkpoints frozen',
        'metric':'sum confusion matrices over every test pixel, then average IoU of classes with nonzero union',
        'majority_class_test':scores(confusion(masks[te],majority)),'runs':runs,'seconds':time.monotonic()-began,
        'limitations':'filename groups are conservative proxies; unknown scene identities and near-duplicates can remain',
        'environment':{'python':platform.python_version(),'torch':torch.__version__,'numpy':np.__version__},
        'code_sha256':{n:hashlib.sha256((Path(__file__).parent/n).read_bytes()).hexdigest() for n in ['run.py','car_data.py','segment_model.py']}}
    (args.output/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
    # Only label maps, not the third-party source photographs, are included in the exhibit.
    fig,axes=plt.subplots(3,6,figsize=(12,6),layout='constrained');cmap=ListedColormap(['#f5f1e8','#243c45','#b9a77d','#d49435','#5e8d99'])
    for col,(index,unet,pixel) in enumerate(zip(te[:6],predictions['U-Net'][:6],predictions['Color-only network'][:6])):
        for row,mask in enumerate([masks[index],pixel,unet]):
            axes[row,col].imshow(mask,cmap=cmap,vmin=0,vmax=4);axes[row,col].set_xticks([]);axes[row,col].set_yticks([])
        axes[0,col].set_title(f'Test image {col+1}',fontsize=9)
    for ax,label in zip(axes[:,0],['True labels','Color only','U-Net']):ax.set_ylabel(label)
    fig.suptitle('First six test images in filename order · real course labels, resized to 96 × 96')
    fig.savefig(args.output/'masks.png',dpi=150);plt.close(fig)
    fig,ax=plt.subplots(figsize=(8,4),layout='constrained')
    for run in runs:ax.plot([h['epoch'] for h in run['history']],[h['validation_mean_iou'] for h in run['history']],label=run['model'])
    ax.set(xlabel='Epoch',ylabel='Validation pooled mean IoU',ylim=(0,1));ax.legend();fig.savefig(args.output/'training.png',dpi=150);plt.close(fig)
    print(json.dumps({'counts':report['counts'],'results':[{k:v for k,v in r.items() if k not in ['history','test_by_image']} for r in runs]},indent=2))

if __name__=='__main__':main()
