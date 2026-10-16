import sys,re
sys.argv_=sys.argv
exec(open('/tmp/perf/tool/an2.py').read().split("if __name__")[0])
for run in sys.argv[1:]:
    recs=load(run); sc=scenes(run); print("==",run)
    for name,w0,w1,cpu in sc:
        if name not in ("first","stand"): continue
        r=[x for x in recs if w0<=x[0]<=w1]
        if len(r)<5: continue
        out=[]
        for i in range(1,len(r)):
            d=r[i][0]-r[i-1][0]
            if d>50: out.append("%.1fs:%.0fms(draw %.0f,lock %.0f,create %d)"%((r[i][0]-w0)/1000,d,r[i][1][2],r[i][1][4],r[i][3][19]+r[i][3][22]+r[i][3][23]))
        print("  %s %d hitches: %s"%(name,len(out)," ".join(out[:14])))
