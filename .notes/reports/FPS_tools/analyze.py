import os
D=os.environ.get("D","/tmp/d3f-out")
import sys,re,struct,glob,os,statistics
def load(run):
    b=open(D+"/%s.present.bin"%run,"rb").read(); global RECS
    if int(re.match(r"r(\d+)",run).group(1))<400:
        n=len(b)//8; RECS=[]; return list(struct.unpack("%dd"%n,b[:n*8]))
    n=len(b)//48
    RECS=[struct.unpack_from("<d8fII",b,i*48) for i in range(n)]
    return [r[0] for r in RECS]
RECS=[]
def scenes(run):
    out=[]
    for l in open(D+"/%s.txt"%run,errors="replace"):
        m=re.search(r"\[scene (\S+)\] ([\d.]+) ([\d.]+)(?: cpu_ms=(-?\d+))?",l)
        if m: out.append((m.group(1).replace(run+"_",""),float(m.group(2)),float(m.group(3)),float(m.group(4)) if m.group(4) else None))
    return out
import time
def hostrows():
    rows=[]
    for l in open(D+"/hostload.log",errors="replace"):
        m=re.match(r"(\d+):(\d+):(\d+) CPU usage: ([\d.]+)% user, ([\d.]+)% sys, ([\d.]+)% idle.*load \{ ([\d.]+)",l)
        if m: rows.append((int(m.group(1))*3600+int(m.group(2))*60+int(m.group(3)),float(m.group(6)),float(m.group(7))))
    return rows
def cpuwin(run,w0,w1):
    import re
    def secs(s):
        if not s: return None
        m=re.match(r"(?:(\d+):)?(\d+):([\d.]+)$",s)
        if not m: return None
        return int(m.group(1) or 0)*3600+int(m.group(2))*60+float(m.group(3))
    u0=(w0-11644473600000)/1000.0; u1=(w1-11644473600000)/1000.0; rows=[]
    try:
        for l in open(D+"/%s.cpu"%run):
            p=l.split()
            if len(p)>=2: rows.append((float(p[0]),secs(p[1]),secs(p[2]) if len(p)>2 else None))
    except OSError: return (None,None)
    r=[x for x in rows if u0-1<=x[0]<=u1+1 and x[1] is not None]
    if len(r)<3: return (None,None)
    dt=r[-1][0]-r[0][0]
    gc=(r[-1][1]-r[0][1])/dt*100
    ws=[x for x in r if x[2] is not None]
    wc=((ws[-1][2]-ws[0][2])/dt*100) if len(ws)>=3 else None
    return (r[-1][1]-r[0][1], wc, dt)
def hostwin(rows,w0,w1):
    lt=lambda w: time.localtime((w-11644473600000)/1000.0)
    a=lt(w0); b=lt(w1); sa=a.tm_hour*3600+a.tm_min*60+a.tm_sec; sb=b.tm_hour*3600+b.tm_min*60+b.tm_sec
    r=[x for x in rows if sa<=x[0]<=sb]
    if not r: return (None,None)
    return (sum(x[1] for x in r)/len(r), sum(x[2] for x in r)/len(r))
def cpuf(run,w0,w1,st):
    r=cpuwin(run,w0,w1)
    if r[0] is None: return ('?','?','?')
    gcpu,wc,dt=r
    return ('%.1fms'%(gcpu*1000/(st['n']+1)),'%.0f%%'%(gcpu/dt*100),'%.0f%%'%wc if wc is not None else '?')
def stats(ts,w0,w1):
    p=[t for t in ts if w0<=t<=w1]
    d=[p[i+1]-p[i] for i in range(len(p)-1)]
    if len(d)<5: return None
    s=sorted(d); n=len(s); dur=(p[-1]-p[0])/1000.0
    h=[(p[i+1]-w0)/1000.0 for i in range(len(p)-1) if d[i]>50]
    return dict(n=n,fps=n/dur,p50=s[n//2],p95=s[int(n*.95)],p99=s[int(n*.99)],mx=s[-1],g50=sum(1 for x in d if x>50),g100=sum(1 for x in d if x>100),dur=dur,hitch_t=h,
                per_min50=sum(1 for x in d if x>50)*60/dur, per_min100=sum(1 for x in d if x>100)*60/dur)
if __name__=="__main__":
    for run in sys.argv[1:]:
        ts=load(run)
        print("==",run); rows=hostrows()
        for name,w0,w1,cpu in scenes(run):
            st=stats(ts,w0,w1)
            if not st: print("  %-10s no data"%name); continue
            print("  %-10s fps=%6.1f p50=%5.1f p95=%6.1f p99=%6.1f max=%6.1f ms  >50:%3d (%.1f/min) >100:%3d (%.1f/min) dur=%.0fs idle=%s load=%s cpu/frame=%s gameCPU=%s wineserverCPU=%s"%(name,st['fps'],st['p50'],st['p95'],st['p99'],st['mx'],st['g50'],st['per_min50'],st['g100'],st['per_min100'],st['dur'],*[('%.0f'%v if v is not None else '?') for v in hostwin(rows,w0,w1)], *cpuf(run,w0,w1,st)))
