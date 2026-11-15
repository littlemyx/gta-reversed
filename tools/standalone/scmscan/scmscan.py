#!/usr/bin/env python3
"""scmscan: static opcode usage of main.scm (+ script.img scripts) joined with the C++ command registration table.
Arg type bytes mirror source/game_sa/Scripts/RunningScript.h:19-52 (eScriptParameterType) and the readers in
source/game_sa/Scripts/CommandParser/ReadArg.hpp:120-200. The SCM carries no per-opcode arg counts, so counts are
learned: seeded from the executed-instruction deltas of the two scmtraces, then refined by lookahead voting (learn()).
Usage: scmscan.py [--data DIR] [--repo DIR] [--out TSV] [--traces F...]"""
import struct, re, sys, os, gzip, glob, collections, argparse

SZ = {1: 4, 2: 2, 3: 2, 4: 1, 5: 2, 6: 4, 7: 6, 8: 6, 9: 8, 0xA: 2, 0xB: 2, 0xC: 6, 0xD: 6, 0xF: 16,
      0x10: 2, 0x11: 2, 0x12: 6, 0x13: 6}


def arg_len(d, p, end):
    """One typed arg at p. Returns (newp, is_end_marker) or None if invalid."""
    if p >= end: return None
    t = d[p]; p += 1
    if t == 0: return p, True
    if t == 0xE:  # pascal string: u8 len + data
        if p >= end: return None
        return p + 1 + d[p], False
    if t not in SZ: return None
    return p + SZ[t], False


def parse_args(d, p, n, end):
    """n>=0: fixed count; n==-1: variadic (until type 0, consumed). Returns end ptr or None."""
    if n == -1:
        for _ in range(64):
            r = arg_len(d, p, end)
            if r is None: return None
            p, e = r
            if e: return p
        return None
    for _ in range(n):
        r = arg_len(d, p, end)
        if r is None or r[1]: return None
        p = r[0]
    return p


def load_enum(repo):
    names = {}
    txt = open(repo + '/source/game_sa/Enums/eScriptCommands.h', encoding='utf8', errors='replace').read()
    for m in re.finditer(r'^\s*(COMMAND_\w+)\s*=\s*0x([0-9A-Fa-f]+)', txt, re.M):
        names[int(m.group(2), 16)] = m.group(1)
    return names


def load_registration(repo, names):
    """status per opcode: implemented / NOP / UNIMPLEMENTED / UNSUPPORTED (stub); absent = unregistered."""
    n2o = {v: k for k, v in names.items()}
    st = {}
    pat = re.compile(r'REGISTER_(COMMAND_HANDLER|COMMAND_UNIMPLEMENTED|COMMAND_NOP|UNSUPPORTED_COMMAND_HANDLER)\(\s*(COMMAND_\w+)')
    kind = {'COMMAND_HANDLER': 'implemented', 'COMMAND_UNIMPLEMENTED': 'UNIMPLEMENTED', 'COMMAND_NOP': 'NOP',
            'UNSUPPORTED_COMMAND_HANDLER': 'UNSUPPORTED'}
    for f in glob.glob(repo + '/source/game_sa/Scripts/**/*.cpp', recursive=True):
        for line in open(f, encoding='utf8', errors='replace'):
            if line.lstrip().startswith('//'): continue
            m = pat.search(line)
            if m and m.group(2) in n2o:
                o = n2o[m.group(2)]
                if st.get(o) in (None, 'UNIMPLEMENTED') or kind[m.group(1)] != 'UNIMPLEMENTED':
                    st[o] = kind[m.group(1)]
    return st


class Seg:
    def __init__(s, name, d, start, end, kind): s.name, s.d, s.start, s.end, s.kind = name, d, start, end, kind


def segments(data):
    main = open(data + '/data/script/main.scm', 'rb').read()
    segs = []
    base = 0xcfa4  # segment-3 header (goto + chunk index = 8 bytes), tSCMScriptFileInfoChunk in SCMChunks.hpp:53
    main_size, largest, nm, nex, lv = struct.unpack_from('<IIHHI', main, base + 8)
    offs = list(struct.unpack_from('<%dI' % nm, main, base + 8 + 16))
    code0 = struct.unpack_from('<I', main, base + 3)[0]
    while main[code0:code0 + 3] == bytes([2, 0, 1]):  # follow the leading GOTO chain (data blobs sit between)
        code0 = struct.unpack_from('<I', main, code0 + 3)[0]
    segs.append(Seg('main', main, code0, main_size, 'main'))
    for i, o in enumerate(offs):
        e = offs[i + 1] if i + 1 < nm else len(main)
        segs.append(Seg('m%d' % i, main, o, e, 'mission'))
    img = data + '/data/script/script.img'
    if os.path.exists(img):
        b = open(img, 'rb').read()
        if b[:4] == b'VER2':
            n = struct.unpack_from('<I', b, 4)[0]
            for i in range(n):
                off, sa, sb, nm_ = struct.unpack_from('<IHH24s', b, 8 + 32 * i)
                nm_ = nm_.split(b'\0')[0].decode('latin1')
                sz = (sa or sb) * 2048
                segs.append(Seg('s:' + nm_, b[off * 2048: off * 2048 + sz], 0, sz, 'streamed'))
    return segs


def decode(seg, counts, votes=None, look=10):
    """Linear decode. Returns (list of (ip, op, negated, argsend), desyncs)."""
    d, p, end = seg.d, seg.start, seg.end
    out, desync = [], []

    def chain_score(p, depth):
        """-1 if the chain hits an invalid arg/opcode; else number of known instructions parsed (max depth)."""
        k = 0
        while k < depth:
            if p + 2 > end: return depth
            op = struct.unpack_from('<H', d, p)[0] & 0x7fff
            if op not in counts: return k if op < 0x0B00 else -1
            q = parse_args(d, p + 2, counts[op], end)
            if q is None: return -1
            p, k = q, k + 1
        return k

    while p + 2 <= end:
        raw = struct.unpack_from('<H', d, p)[0]; op = raw & 0x7fff
        if op in counts:
            q = parse_args(d, p + 2, counts[op], end)
            if q is None:
                desync.append((p, op)); break
        else:
            q, best, bn = None, -1, None
            for n in list(range(0, 16)) + [-1]:
                qq = parse_args(d, p + 2, n, end)
                if qq is None: continue
                sc = chain_score(qq, look)
                if sc > best: best, q, bn = sc, qq, n
            if q is not None and votes is not None and best >= look: votes[op][bn] += 1
            if q is None:
                desync.append((p, op)); break
        out.append((p, op, bool(raw & 0x8000), q)); p = q
    return out, desync


def seed_from_traces(sbn, paths):
    votes = collections.defaultdict(collections.Counter)
    seen = set()
    for pth in paths:
        opener = gzip.open if pth.endswith('.gz') else open
        prev = {}
        with opener(pth, 'rt', errors='replace') as f:
            for line in f:
                if line[0] != 'X': continue
                c = line.split('\t')
                if len(c) < 8: continue
                th, base, ip, opc = c[2], c[4], int(c[5], 16), int(c[6], 16) & 0x7fff
                seen.add(opc)
                pv = prev.get(th)
                if pv and pv[0] == base and pv[1] < ip < pv[1] + 400:
                    sg = sbn.get(base)
                    if sg is not None and sg.kind != 'streamed':
                        o0 = sg.start + pv[1] if sg.kind == 'mission' else pv[1]
                        tgt = sg.start + ip if sg.kind == 'mission' else ip
                        votes[pv[2]]['tot'] += 1
                        for n in list(range(0, 14)) + [-1]:
                            if parse_args(sg.d, o0 + 2, n, sg.end) == tgt:
                                votes[pv[2]][n] += 1; break
                prev[th] = (base, ip, opc)
    return votes, seen


def learn(segs, sbn, traces):
    counts = {}
    tv, seen = seed_from_traces(sbn, traces)
    for op in list(tv):
        c = tv[op]; tot = c.pop('tot', 0)
        if c:
            n, k = c.most_common(1)[0]
            if k >= 2 and k >= 0.6 * tot: counts[op] = n; continue
        del tv[op]  # jump-like / too few fall-through samples: learned by lookahead instead
    print('trace-seeded counts', len(counts), file=sys.stderr)
    for it in range(8):
        votes = collections.defaultdict(collections.Counter)
        for s in segs: decode(s, counts, votes)
        ch = 0
        for op, c in votes.items():
            if op in tv: continue
            n = c.most_common(1)[0][0]
            if counts.get(op) != n: counts[op] = n; ch += 1
        print('learn iter', it, 'new/changed', ch, 'known', len(counts), file=sys.stderr)
        if not ch: break
    return counts, seen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default='C:/gta-scratch/scmtrace/d')
    ap.add_argument('--repo', default=os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..')))
    ap.add_argument('--out', default=None)
    ap.add_argument('--traces', nargs='*', default=['C:/gta-scratch/scmtrace/out/full/record.scmtrace.gz',
                                                    'C:/gta-scratch/scmtrace/out/catalin/catalin.scmtrace'])
    a = ap.parse_args()
    out = a.out or a.repo + '/.notes/asm_sweep/T3_SCM_USAGE.tsv'
    names = load_enum(a.repo); reg = load_registration(a.repo, names)
    segs = segments(a.data)
    sbn = {s.name: s for s in segs}
    counts, seen = learn(segs, sbn, [t for t in a.traces if os.path.exists(t)])
    use = collections.defaultdict(lambda: [0, set()])
    nd = 0
    for s in segs:
        ins, ds = decode(s, counts)
        if ds:
            nd += 1; print('DESYNC', s.name, [(hex(p), hex(o)) for p, o in ds], 'decoded', len(ins), file=sys.stderr)
        for ip, op, neg, q in ins:
            use[op][0] += 1; use[op][1].add(s.name)
    print('segments', len(segs), 'desynced', nd, 'opcodes learned', len(counts), 'used', len(use), file=sys.stderr)
    with open(out, 'w') as f:
        f.write('opcode\tname\tcpp_status\tstatic_count\tmissions_using\tseen_in_trace\targ_count\n')
        for op in sorted(use, key=lambda o: (-len(use[o][1]), -use[o][0])):
            ms = sorted(use[op][1])
            f.write('0x%04X\t%s\t%s\t%d\t%s\t%s\t%s\n' % (
                op, names.get(op, '?'), reg.get(op, 'unregistered'), use[op][0],
                ','.join(ms) if len(ms) <= 12 else '%d segments (%s,...)' % (len(ms), ','.join(ms[:4])),
                'yes' if op in seen else 'no', counts.get(op, '?')))
    print('wrote', out, file=sys.stderr)


if __name__ == '__main__': main()
