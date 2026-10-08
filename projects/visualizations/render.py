"""Portable reconstructions of three plotting exercises from the coursework archive (2019)."""
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter


def main():
    out = Path(__file__).parent / "results"; out.mkdir(exist_ok=True)
    plt.rcParams.update({"font.family":"DejaVu Sans", "axes.spines.top":False, "axes.spines.right":False})
    fig = plt.figure(figsize=(11,4.5),layout="constrained",facecolor="#f5f1e8")
    ax = fig.add_subplot(121)
    # Uniform sampling cannot resolve infinitely many oscillations near zero.
    # Sample a bounded positive interval and draw the continuous value at zero.
    x = np.linspace(.001,.15,24000)
    ax.plot(x,x*np.sin(1/x),color="#a64d35",linewidth=.8)
    ax.plot([0,.15],[0,.15],color="#b9a77d",linestyle="--",linewidth=.7)
    ax.plot([0,.15],[0,-.15],color="#b9a77d",linestyle="--",linewidth=.7)
    ax.scatter([0],[0],s=12,color="#243c45")
    ax.set(xlabel="x",ylabel="f(x)",title="Oscillation inside a shrinking envelope")
    ax = fig.add_subplot(122,projection="3d")
    grid = np.linspace(-2,2,65);xx,yy = np.meshgrid(grid,grid)
    ax.plot_surface(xx,yy,xx**2-yy**2,cmap="copper",linewidth=0,antialiased=True)
    ax.set(xlabel="x",ylabel="y",zlabel="z",title="The saddle: z = x² − y²")
    ax.view_init(25,-50)
    fig.savefig(out/"gallery.png",dpi=180);plt.close(fig)
    fig,ax=plt.subplots(figsize=(7,2.5),layout="constrained",facecolor="#f5f1e8")
    x=np.linspace(0,4*np.pi,500);line,=ax.plot(x,np.sin(x),color="#243c45")
    ax.set(xlim=(0,4*np.pi),ylim=(-1.15,1.15),xlabel="x",ylabel="sin(ωx)")
    title=ax.set_title("Frequency sweep")
    def update(frame):
        omega=1+.6*np.sin(2*np.pi*frame/40)
        line.set_ydata(np.sin(omega*x));title.set_text(f"Frequency sweep · ω = {omega:.2f}")
        return line,title
    animation=FuncAnimation(fig,update,frames=40,interval=80)
    animation.save(out/"frequency.gif",writer=PillowWriter(fps=12));plt.close(fig)
    print("Rendered gallery.png and frequency.gif")


if __name__ == "__main__": main()
