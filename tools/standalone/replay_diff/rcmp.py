"""REPLAY-DIFF R-log comparer (A-B noise hunting): per traced frame, a table "count | site" for the rand() calls of an ORIGINAL trace (rd_orig.c "R fc retaddr" lines; return address -> registered
function of the port's hook registry, RH_Scoped*Install(Name, 0xADDR) lines + the RH_ScopedClass of the file) and of a PORT trace ("R fc file:line [+rva]" lines; General.cpp sites resolved to the caller
function through the pdb, like resolve_stack.py). Frame k = the R lines before the k-th F line.
usage: py -I rcmp.py <repo> <gta_reversed.exe> <orig.trace> <port.trace> <frameA> <frameB> [--seq]     --seq: also print the call sequence of each side"""
import sys, re, glob, os, bisect, collections, subprocess
repo, exe, ot, pt, fa, fb = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]), int(sys.argv[6])
seq = '--seq' in sys.argv
reg = {}
for f in glob.glob(os.path.join(repo, 'source', '**', '*.cpp'), recursive=True):
    cls = ''
    for ln in open(f, errors='replace'):
        m = re.search(r'RH_ScopedClass\((\w+)\)', ln)
        if m: cls = m.group(1)
        m = re.search(r'RH_Scoped(\w*)Install\(([^,]+),\s*0x([0-9A-Fa-f]{6})', ln)
        if m: reg[int(m.group(3), 16)] = ('' if 'Global' in m.group(1) else cls + '::') + m.group(2).strip()
keys = sorted(reg)
def orig_name(a):
    i = bisect.bisect_right(keys, a) - 1
    return '%s+%x' % (reg[keys[i]], a - keys[i]) if i >= 0 else '?%x' % a
def frames(path, a, b, orig):
    out = collections.defaultdict(list); k = 0
    for ln in open(path, errors='replace'):
        if ln[0] == 'F':
            k += 1
            if k > b: break
        elif ln[0] == 'R' and a <= k + 1 <= b:
            t = ln.split()
            out[k + 1].append(t[2] if orig else (t[2], [int(x[1:], 16) for x in t[3:] if x.startswith('+')]))
    return out
O = frames(ot, fa, fb, True); P = frames(pt, fa, fb, False)
# port: resolve +rva with resolve_stack.py machinery (write a tiny R-file per frame and call it)
def port_names(items):
    tmp = os.path.join(os.environ.get('TEMP', '.'), 'rcmp_tmp.trace')
    with open(tmp, 'w') as w:
        for s, offs in items:
            w.write('R 1 %s %s\n' % (s, ' '.join('+%x' % o for o in offs)))
    r = subprocess.run([sys.executable, '-I', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'resolve_stack.py'), exe, tmp, '--depth', '1'], capture_output=True, text=True).stdout
    return r
for k in range(fa, fb + 1):
    print('=== frame %d: orig %d calls, port %d calls' % (k, len(O[k]), len(P[k])))
    co = collections.Counter(re.sub(r'\+[0-9a-f]+$', '', orig_name(int(x, 16))) for x in O[k])
    cp = collections.Counter(s if not offs else s + ' ' + str(offs[0]) for s, offs in P[k])
    print(' orig by function:'); [print('  %4d %s' % (c, n)) for n, c in co.most_common()]
    print(' port by site (General.cpp sites: +rva of the caller; resolve with resolve_stack.py):'); [print('  %4d %s' % (c, n)) for n, c in cp.most_common()]
    if seq:
        print(' orig seq:', ' '.join(re.sub(r'\+[0-9a-f]+$', '', orig_name(int(x, 16))) for x in O[k]))
        print(' port seq:', ' '.join(s if not offs else '%s:+%x' % (s, offs[0]) for s, offs in P[k]))
