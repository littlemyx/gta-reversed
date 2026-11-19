import struct,sys
d=open('C:/Users/littl/github/gta-reversed/gta_sa_compact.exe','rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]
nsec=struct.unpack_from('<H',d,pe+6)[0]; opt=struct.unpack_from('<H',d,pe+20)[0]
base=struct.unpack_from('<I',d,pe+24+28)[0]
secs=[]
for i in range(nsec):
    o=pe+24+opt+i*40
    vs,va,rs,ro=struct.unpack_from('<IIII',d,o+8)
    secs.append((va,vs,ro,rs))
targets={int(t,16):[] for t in sys.argv[1:]}
for va,vs,ro,rs in secs:
    for i in range(min(vs,rs)-5):
        if d[ro+i]==0xE8:
            rel=struct.unpack_from('<i',d,ro+i+1)[0]
            tgt=(base+va+i+5+rel)&0xffffffff
            if tgt in targets: targets[tgt].append(base+va+i)
for t,l in targets.items():
    print(hex(t),len(l)); print(' '.join(hex(x) for x in l))
