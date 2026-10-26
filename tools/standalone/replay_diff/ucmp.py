import sys,struct
f=lambda h:struct.unpack('<f',struct.pack('<I',int(h,16)))[0]
def load(p):
    d={}
    for ln in open(p):
        if ln.startswith('X') and ' U ' in ln:
            t=ln.split(); d[(int(t[1]),t[3])]=(t[4],t[6:])
    return d
O=load(sys.argv[1]);P=load(sys.argv[2]); only=sys.argv[3:] 
for k in sorted(O):
    if only and k[1] not in only: continue
    if k not in P: print(k,'missing in port'); continue
    o,p=O[k],P[k]
    if o[0]!=p[0]: print(k,'pos',o[0],p[0])
    for i,(a,b) in enumerate(zip(o[1],p[1])):
        if a!=b: print(k,'+0x%x'%(0x1c+4*i),a,b,'(%g %g)'%(f(a),f(b)) if True else '')
