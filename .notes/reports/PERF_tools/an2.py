import sys,re,struct,os
D="/tmp/perf/out"
RS=8+32+8+96
names=["DP","DIP","DPUP","DIPUP","SetTex","SetRS","SetTSS","SetSamp","SetXform","SetVDecl/FVF","SetVS","SetPS","SetStream","SetIdx","VSConst","PSConst","LockTex","LockVB","LockIB","Create","VBbytes","IBbytes","CreateIB","CreateVB"]
def load(run):
    p="%s/%s.present.bin"%(D,run)
    if not os.path.exists(p): return []
    b=open(p,"rb").read(); n=len(b)//RS; out=[]
    for i in range(n):
        ts,=struct.unpack_from("<d",b,i*RS); bt=struct.unpack_from("<8f",b,i*RS+8); nd,pad=struct.unpack_from("<II",b,i*RS+40); c=struct.unpack_from("<24I",b,i*RS+48)
        out.append((ts,bt,nd,c))
    return out
def scenes(run):
    out=[]; thr={}
    for l in open("%s/%s.txt"%(D,run),errors="replace"):
        m=re.search(r"\[scene (\S+)\] ([\d.]+) ([\d.]+) cpu_ms=(-?\d+)",l)
        if m: out.append((m.group(1).replace(run+"_",""),float(m.group(2)),float(m.group(3)),float(m.group(4))))
    return out
def secs(s):
    m=re.match(r"(?:(\d+):)?(\d+):([\d.]+)$",s)
    return int(m.group(1) or 0)*3600+int(m.group(2))*60+float(m.group(3))
def cpurows(run):
    rows={}
    try:
        for l in open("%s/%s.cpu"%(D,run)):
            p=l.split()
            if len(p)<4 or not re.match(r'^\d+:',p[2]) or not re.match(r'^\d+:',p[3]): continue
            rows.setdefault(float(p[0]),{})[int(p[1])]=secs(p[2])+secs(p[3])
    except OSError: pass
    return rows
def report(run):
    recs=load(run); sc=scenes(run); rows=cpurows(run); ts=sorted(rows); print("==",run)
    for name,w0,w1,cpu in sc:
        if name=="menu": continue
        r=[x for x in recs if w0<=x[0]<=w1]
        line="  %-8s"%name
        n=0
        if len(r)>=5:
            n=len(r)-1; dts=[r[i+1][0]-r[i][0] for i in range(n)]; s=sorted(dts)
            line+=" n=%d fps=%.1f p50=%.1f p95=%.1f >50=%d"%(n,n*1000/(r[-1][0]-r[0][0]),s[n//2],s[int(n*.95)],sum(1 for d in dts if d>50))
            tot=[0]*24; bt=[0]*8
            for x in r[1:]:
                for k in range(24): tot[k]+=x[3][k]
                for k in range(8): bt[k]+=x[1][k]
        u0=(w0-11644473600000)/1000.0; u1=(w1-11644473600000)/1000.0
        a=[t for t in ts if t>=u0-0.6]; b=[t for t in ts if t<=u1+0.6]
        if a and b and b[-1]>a[0]+3:
            t0=a[0]; t1=b[-1]
            if n==0:
                # frame count unknown: use present records not available -> parse "[thr" frames
                pass
            d=[]
            for k in rows[t1]:
                if k in rows[t0]:
                    v=rows[t1][k]-rows[t0][k]
                    if v>0.05: d.append((k,v))
            nn=n if n else 1
            line+="\n      cpu(%.0fs): "%(t1-t0)+" ".join("th%d=%.0f%%"%(k,v/(t1-t0)*100) for k,v in d)
            if n: line+="  | ms/frame: "+" ".join("th%d=%.1f"%(k,v*1000/ ((t1-t0)/ ((r[-1][0]-r[0][0])/1000.0/n)  ) / ((t1-t0))*((t1-t0))/ (n*((t1-t0)/((r[-1][0]-r[0][0])/1000.0)))) for k,v in d if v/(t1-t0)>0.05)
        print(line)
        if len(r)>=5:
            print("     per frame: "+" ".join("%s=%.0f"%(names[k],tot[k]/n) for k in range(23) if tot[k]))
            print("     D3D ms/frame: present=%.1f clear=%.1f draw=%.1f create=%.1f lock=%.1f update=%.1f"%(bt[0]/n,bt[1]/n,bt[2]/n,bt[3]/n,bt[4]/n,bt[5]/n))
if __name__=="__main__":
    for run in sys.argv[1:]: report(run)
