# RD-AB-11: compare the "P fc slot model pos spd rot" lines (pin bit 1024: every used ped-pool slot, each frame) of an orig and a port trace.
# usage: py -I pdiff.py orig.trace port.trace [maxlines]   -> first frame where each (slot) differs, bit-level, with the float values
import struct, sys
f = lambda h: struct.unpack('<f', struct.pack('<I', int(h, 16)))[0]
def load(p):
    d = {}
    for ln in open(p):
        if ln.startswith('P '):
            t = ln.split(); d[(int(t[1]), int(t[2]))] = (t[3], t[4].split(','), t[5].split(','), t[6].split(",")+t[7].split(",")+t[8].split(","))
    return d
O = load(sys.argv[1]); P = load(sys.argv[2]); mx = int(sys.argv[3]) if len(sys.argv) > 3 else 40
seen = {}; n = 0
for k in sorted(O):
    if k not in P:
        print(k, 'orig only', O[k][0]); continue
    a, b = O[k], P[k]
    if a != b and k[1] not in seen:
        seen[k[1]] = k[0]
        names = ['pos', 'spd', 'rot']
        diffs = []
        for i, nm in enumerate(names):
            for j in range(len(a[i + 1])):
                if a[i + 1][j] != b[i + 1][j]:
                    diffs.append('%s%d %s/%s (%.7g/%.7g)' % (nm, j, a[i + 1][j], b[i + 1][j], f(a[i + 1][j]), f(b[i + 1][j])))
        print('slot', k[1], 'model', a[0], b[0], 'first diff fc', k[0], '|', '; '.join(diffs)); n += 1
        if n >= mx: break
for k in sorted(P):
    if k not in O: print(k, 'port only', P[k][0]); break
print('slots differing:', len(seen), 'of', len({k[1] for k in O}))
