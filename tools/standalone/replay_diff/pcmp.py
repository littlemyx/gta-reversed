# RD-AB-10: compare the "X fc S ..." scan-probe lines (rd_orig.c RD_BPSCAN at BP 0x606739 vs a temporary port Note in ScanForPedPotentialCollisionEvents) per (frame, ped model): py -I pcmp.py orig.trace port.trace
import struct,sys
f=lambda h:struct.unpack('<f',struct.pack('<I',int(h,16)))[0]
def load(p):
    d={}
    for ln in open(p):
        if ln.startswith('X') and ' S ' in ln:
            t=ln.split(); fc=int(t[1]); kv=dict(x.split('=') for x in t if '=' in x)
            d.setdefault((fc,kv['m']),kv)
    return d
O=load(sys.argv[1]);P=load(sys.argv[2])
for k in sorted(O):
    if k in P:
        a=O[k];b=P[k]
        print(k,'orig',[round(f(x),4) for x in a['pos'].split(',')],'port',[round(f(x),4) for x in b['pos'].split(',')],'fwd',[round(f(x),3) for x in a['fwd'].split(',')[:2]],[round(f(x),3) for x in b['fwd'].split(',')[:2]], 'same' if a['pos']==b['pos'] else 'DIFF')
    else: print(k,'orig only')
