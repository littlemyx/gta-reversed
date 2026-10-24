#!/usr/bin/env python3
"""lp64_scan.py - MAC-P5: find latent LP64 (arm64 / Itanium ABI) bugs the compiler cannot see in the native macOS build.

  lp64_scan.py layouts [-j N]   dump every complete record of every game TU of build/PortableMac (clang -fdump-record-layouts-complete) and report
                                 TAIL  : a derived class / second base placed in the tail padding of a non-POD base (Itanium dsize < sizeof; MSVC and the
                                         exe never reuse it). Found C2dEffectParticle that way: its name landed at +13 instead of +16 ("cigarette_smoke" ->
                                         "arette_smoke"). Anything printed here is a layout that differs from the exe for reasons unrelated to pointer width.
                                 UNION : unions that overlay a pointer with a 32-bit scalar (the exe writes the whole 4-byte slot, a 4-byte store leaves the
                                         upper half of the 8-byte pointer stale) - review the writers of the scalar side.
  lp64_scan.py offsets          grep game_sa/app/toolsmenu/portable for byte-pointer casts + constant offsets (`(uint8*)p + 0x2CC`, `*(T*)((char*)x + N)`),
                                 literal memcpy/memset sizes, `this` + N hacks, `(WORD*)p)[N]` indexing: x86 offsets into classes that hold pointers are wrong on LP64.
Needs build/PortableMac/compile_commands.json and brew LLVM.  Output is a candidate list, not a verdict.
"""
import concurrent.futures as cf, json, os, pickle, re, shlex, subprocess, sys, tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def jobs():
    cc = json.load(open(REPO + '/build/PortableMac/compile_commands.json'))
    return [e for e in cc if 'gta_reversed.dir' in e['command'] and any(d in e['file'] for d in ('/source/game_sa/', '/source/app/', '/source/toolsmenu/', '/source/portable/'))]


def argv_for(e):
    t = shlex.split(e['command']); out = []; i = 0
    while i < len(t):
        a = t[i]
        if a in ('-MD', '-Winvalid-pch', '-fpch-instantiate-templates'): i += 1; continue
        if a in ('-MT', '-MF', '-o', '-c'): i += 2; continue
        if a == '-Xclang' and t[i + 1] == '-include-pch': i += 4; continue   # a PCH hides the records from the dump
        out.append(a); i += 1
    return out + ['-fsyntax-only', '-ferror-limit=0', '-Xclang', '-fdump-record-layouts-complete', e['file']]


HDR = re.compile(r'^\*\*\* Dumping AST Record Layout\n', re.M)


def work(e):
    try:
        p = subprocess.run(argv_for(e), cwd=e['directory'], capture_output=True, timeout=900)
    except Exception:
        return {}
    res = {}
    for ch in HDR.split(p.stdout.decode('utf8', 'replace')):
        m = re.match(r'\s*\d+ \| (.*)$', ch.split('\n', 1)[0])
        if m and 'sizeof=' in ch:
            res[m.group(1)] = ch
    return res


def cmd_layouts(nj):
    allr = {}
    with cf.ThreadPoolExecutor(nj) as ex:
        for r in ex.map(work, jobs()):
            for n, c in r.items():
                allr.setdefault(n, c)
    info, recs = {}, {}
    for name, chunk in allr.items():
        n = re.sub(r'^(struct|class|union) ', '', name)
        m = re.search(r'sizeof=(\d+), dsize=(\d+)', chunk) or re.search(r'sizeof=(\d+)', chunk)
        size = int(m.group(1)); ds = int(m.group(2)) if m.lastindex >= 2 else size
        ch = []
        for ln in chunk.split('\n')[1:]:
            mm = re.match(r'\s*(\d+)(?::\d+-\d+)? \|( *)(.*)$', ln)
            if mm: ch.append((len(mm.group(2)), int(mm.group(1)), mm.group(3)))
        info[n] = (size, ds)
        recs[n] = (len(re.match(r'\s*\d+ \|( *)', chunk.split('\n')[0]).group(1)), ch, name.startswith('union'))
    print('%d records' % len(recs))
    for n, (ind, ch, isu) in recs.items():
        if n.startswith(('std::', '__')): continue
        kids = [c for c in ch if c[0] == ind + 2]
        for i, o, t in kids:
            if not t.endswith('(base)'): continue
            bn = re.sub(r'^(struct|class|union) ', '', t[:-6].strip())
            if bn not in info or info[bn][1] >= info[bn][0]: continue
            s, d = info[bn]
            for i2, o2, t2 in kids:
                if (i2, o2, t2) != (i, o, t) and o + d <= o2 < o + s:
                    print('TAIL  %s: base %s (sizeof %d, dsize %d) at %d is overlapped by [%d] %s' % (n, bn, s, d, o, o2, t2))
        if isu:
            ptr = [t for i, o, t in kids if re.search(r'\*\s+\w+$', t)]
            small = [t for i, o, t in kids if re.match(r'(uint32|int32|u?int|DWORD|unsigned int|uint16|int16|float)\b', t)]
            if ptr and small:
                print('UNION %s ptr: %s  scalar: %s' % (n, ptr[:2], small[:3]))


CAST = re.compile(r'(reinterpret_cast<\s*(?:const\s+)?(?:u?int8|char|std::byte|unsigned char|uint8_t|int8_t|BYTE)\s*\*\s*>|\(\s*(?:const\s+)?(?:u?int8|char|BYTE|unsigned char|uint8_t)\s*\*\s*\))')
PLUS = re.compile(r'\+\s*(0x[0-9A-Fa-f]+|\d+)\b')
MEM = re.compile(r'\b(?:std::)?(?:memcpy|memset|memmove)\s*\([^;]*,\s*(0x[0-9A-Fa-f]{2,}|\d{2,})\s*\)')
IDX = re.compile(r'\(\s*\(\s*(?:u?int(?:8|16|32|64)|char|short|int|float|DWORD|WORD|BYTE)\s*\*\s*\)\s*[\w>.\-]+\s*\)\s*\[\s*\d+\s*\]')
SKIP = ('RenderWare/rw/', 'source/libs', 'source/standalone/rw', 'source/fakerw', 'vendor', 'Audio/Hardware')


def cmd_offsets():
    for root in ('source/game_sa', 'source/app', 'source/toolsmenu', 'source/portable'):
        for dp, dn, fn in os.walk(os.path.join(REPO, root)):
            for f in fn:
                if not f.endswith(('.cpp', '.h', '.hpp', '.inl')): continue
                p = os.path.join(dp, f)
                if any(s in p for s in SKIP): continue
                for n, l in enumerate(open(p, errors='replace'), 1):
                    if l.lstrip().startswith('//'): continue
                    if (CAST.search(l) and PLUS.search(l)) or ('this' in l and re.search(r'\bthis\)\s*\+\s*(0x[0-9A-Fa-f]+|\d+)', l)) or MEM.search(l) or IDX.search(l):
                        print('%s:%d: %s' % (os.path.relpath(p, REPO), n, l.strip()[:180]))


if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == 'layouts':
        cmd_layouts(int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[2] == '-j' else 12)
    elif len(sys.argv) >= 2 and sys.argv[1] == 'offsets':
        cmd_offsets()
    else:
        print(__doc__)
