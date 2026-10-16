import sys,re
# pz5.py RUN NAME : per snapshot (1s) series of calls/frame and incl ms/frame of NAME, with scene tags
D="/tmp/perf/out"
run=sys.argv[1]; names=sys.argv[2:]
sc=[]
for l in open("%s/%s.txt"%(D,run),errors="replace"):
    m=re.search(r"\[scene (\S+)\] ([\d.]+) ([\d.]+)",l)
    if m: sc.append((m.group(1).replace(run+"_",""),float(m.group(2)),float(m.group(3))))
snaps=[];cur=None
for l in open("%s/%s.fprof.log"%(D,run),errors="replace"):
    if l.startswith("T "):
        p=l.split(); cur=(float(p[1]),int(p[2]),{}); snaps.append(cur)
    elif l.startswith("S ") and cur:
        p=l.rstrip("\n").split(" "); name=" ".join(p[2:-3]); cur[2][name]=(int(p[-3])/1000.0,int(p[-1]))
prev=None
for s in snaps:
    if prev and s[1]>prev[1]:
        nf=s[1]-prev[1]
        tag=[n for n,w0,w1 in sc if w0<=s[0]<=w1+1000]
        if tag:
            out=[]
            for n in names:
                a=prev[2].get(n,(0,0)); b=s[2].get(n,(0,0))
                out.append("%s: %.1f calls %.2f ms"%(n,(b[1]-a[1])/nf,(b[0]-a[0])/nf))
            print("%-8s t=%.0f nf=%d  %s"%(tag[0],(s[0]-sc[1][1])/1000 if len(sc)>1 else 0,nf," | ".join(out)))
    prev=s
