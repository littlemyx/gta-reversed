import sys,re
# pz4.py RUN : per-scene zone tables (wall windows) from RUN.fprof.log; prints scene->name->(excl,incl,calls) per frame; optionally compare with RUN2
D="/tmp/perf/out"
def load(run):
    sc=[]
    for l in open("%s/%s.txt"%(D,run),errors="replace"):
        m=re.search(r"\[scene (\S+)\] ([\d.]+) ([\d.]+)",l)
        if m: sc.append((m.group(1).replace(run+"_",""),float(m.group(2)),float(m.group(3))))
    snaps=[];cur=None
    for l in open("%s/%s.fprof.log"%(D,run),errors="replace"):
        if l.startswith("T "):
            p=l.split(); cur=(float(p[1]),int(p[2]),{}); snaps.append(cur)
        elif l.startswith("S ") and cur:
            p=l.rstrip("\n").split(" "); name=" ".join(p[2:-3]); calls=int(p[-1]); excl=int(p[-2])/1000.0; incl=int(p[-3])/1000.0
            cur[2][name]=(incl,excl,calls)
    res={}
    for name,w0,w1 in sc:
        if name=="menu": continue
        a=[s for s in snaps if s[0]>=w0]; b=[s for s in snaps if s[0]<=w1]
        if not a or not b: continue
        a=a[0]; b=b[-1]
        nf=b[1]-a[1]
        if nf<=5: continue
        d={}
        for k,(i,e,c) in b[2].items():
            i0,e0,c0=a[2].get(k,(0,0,0))
            d[k]=((i-i0)/nf,(e-e0)/nf,(c-c0)/nf)
        res[name]=(nf,d)
    return res
if __name__=="__main__":
    runs=sys.argv[1:]
    R=[load(r) for r in runs]
    scenes=["stand","walk_a","walk_b","walk_c"]
    names=set()
    for r in R:
        for s in scenes:
            if s in r: names|=set(r[s][1])
    # average walk scenes
    def avg(r,scs,key):
        v=[r[s][1].get(key,(0,0,0)) for s in scs if s in r]
        if not v: return (0,0,0)
        return tuple(sum(x[i] for x in v)/len(v) for i in range(3))
    rows=[]
    for n in names:
        row=[n]
        for r in R:
            row.append(avg(r,["stand"],n)); row.append(avg(r,["walk_a","walk_b","walk_c"],n))
        rows.append(row)
    key=lambda row: -max(x[0] for x in row[1:])
    rows.sort(key=key)
    print("incl ms/frame (calls/frame): for each run: stand | walk(avg a,b,c)")
    hdr="%-40s"%"zone"+"".join(" | %-26s"%(runs[i//2]+(" stand" if i%2==0 else " walk")) for i in range(2*len(R)))
    print(hdr)
    for row in rows[:int(40)]:
        print("%-40s"%row[0][:40]+"".join(" | %6.2f (%6.1f)          "%(x[0],x[2]) for x in row[1:]))
