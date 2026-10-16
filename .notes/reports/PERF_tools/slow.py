import sys,re,collections
D="/tmp/perf/out"
for run in sys.argv[1:]:
    sc=[]
    for l in open("%s/%s.txt"%(D,run),errors="replace"):
        m=re.search(r"\[scene (\S+)\] ([\d.]+) ([\d.]+)",l)
        if m: sc.append((m.group(1).replace(run+"_",""),float(m.group(2)),float(m.group(3))))
    ev=[]
    try:
        for l in open("%s/%s.slow"%(D,run)):
            p=l.split(); ev.append((float(p[0]),int(p[1][1:]),float(p[2])))
    except OSError: print(run,"no slow"); continue
    print("==",run)
    for n,w0,w1 in sc:
        if n=="menu": continue
        c=collections.Counter(); t=collections.Counter()
        for ts,b,ms in ev:
            if w0<=ts<=w1: c[b]+=1; t[b]+=ms
        print("  %-8s"%n," ".join("b%d:%d(%.0fms)"%(b,c[b],t[b]) for b in sorted(c)))
