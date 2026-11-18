#!/usr/bin/env python3
"""gen_t3_usage.py: join scmscan usage with registration status -> .notes/asm_sweep/T3_SCM_USAGE.tsv and a summary on stdout.
usage: gen_t3_usage.py [--data DIR] [--db sa.json] [--exe-counts exe_counts.json]"""
import sys, os, json, argparse, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scmscan as S

ap = argparse.ArgumentParser()
ap.add_argument('--data', default='C:/gta-scratch/scmtrace/d')
ap.add_argument('--db', default='C:/gta-scratch/t3/sa.json')
ap.add_argument('--exe-counts', default='C:/gta-scratch/t3/exe_counts.json')
a = ap.parse_args()
here = os.path.dirname(os.path.abspath(__file__)); repo = os.path.abspath(os.path.join(here, '../../..'))
counts = S.load_counts(here + '/argcounts.tsv')
names = S.load_enum(repo); reg = S.load_registration(repo, names)
segs = S.segments(a.data); sbn = {s.name: s for s in segs}
_, seen = S.seed_from_traces(sbn, [t for t in ['C:/gta-scratch/scmtrace/out/full/record.scmtrace.gz', 'C:/gta-scratch/scmtrace/out/catalin/catalin.scmtrace'] if os.path.exists(t)])
mn = open('C:/gta-scratch/t3/mnames.txt').read().split('\n')
NONSTORY = set(range(0, 11)) | {35, 71} | set(range(113, 135))  # infra/minigame subscripts, CPRACE, DSKOOL, gyms/jobs/odd jobs


def cls(sg):
    if sg.kind == 'main': return 'main'
    if sg.kind == 'streamed': return 'streamed'
    i = int(sg.name[1:]); return 'side' if i in NONSTORY else 'story'


use = collections.defaultdict(lambda: collections.defaultdict(set))
cnt = collections.Counter()
for s in segs:
    ins, _ = S.decode(s, counts)
    for ip, op, neg, q in ins:
        cnt[op] += 1; use[op][cls(s)].add(s.name)
db = {}
for c in json.load(open(a.db))['extensions'][0]['commands']:
    db[int(c['id'], 16)] = c
ex = json.load(open(a.exe_counts))
out = repo + '/.notes/asm_sweep/T3_SCM_USAGE.tsv'
rows = []
for op in sorted(cnt):
    u = use[op]; allm = u['story'] | u['side'] | u['main'] | u['streamed']
    rows.append(dict(op=op, name=names.get(op, db.get(op, {}).get('name', '?')), st=reg.get(op, 'unregistered'), n=cnt[op],
                     story=len(u['story']), side=len(u['side']), main=len(u['main']), strm=len(u['streamed']), seen=op in seen,
                     segs=','.join(sorted(allm)) if len(allm) <= 10 else '%d segments' % len(allm)))
with open(out, 'w', newline='\n') as f:
    f.write('opcode\tname\tcpp_status\tstatic_count\tmissions_using\tseen_in_trace\tstory_missions\tside_missions\tmain\tstreamed\n')
    for r in sorted(rows, key=lambda r: (-r['story'], -r['side'], -r['n'])):
        f.write('0x%04X\t%s\t%s\t%d\t%s\t%s\t%d\t%d\t%d\t%d\n' % (r['op'], r['name'], r['st'], r['n'], r['segs'], 'yes' if r['seen'] else 'no', r['story'], r['side'], r['main'], r['strm']))
# summary
allst = collections.Counter(reg.get(o, 'unregistered') for o in range(0xA4F))
print('registered status over 0..0xA4E:', dict(allst))
for st in ('UNIMPLEMENTED', 'unregistered', 'UNSUPPORTED', 'NOP'):
    rs = [r for r in rows if r['st'] == st]
    print(st, 'total', allst[st], 'used in any script', len(rs), 'in story', sum(1 for r in rs if r['story']), 'in side only', sum(1 for r in rs if not r['story'] and r['side']),
          'main only', sum(1 for r in rs if not r['story'] and not r['side'] and r['main']), 'streamed only', sum(1 for r in rs if not r['story'] and not r['side'] and not r['main'] and r['strm']),
          'static uses', sum(r['n'] for r in rs))
print('used opcodes', len(rows), 'story missions', 135 - len(NONSTORY))
bad = [r for r in rows if r['st'] in ('UNIMPLEMENTED', 'unregistered', 'UNSUPPORTED')]
bad.sort(key=lambda r: (-r['story'], -r['side'] - r['main'] - r['strm'], -r['n']))
json.dump([dict(r, desc=db.get(r['op'], {}).get('short_desc', '')) for r in bad], open('C:/gta-scratch/t3/bad.json', 'w'))
# NOP with real exe handler
nop = []
for r in rows:
    if r['st'] != 'NOP': continue
    e = ex.get('%04X' % r['op'])
    if e and (e[2] or e[3] > 14): nop.append(dict(r, calls=e[2], ni=e[3], desc=db.get(r['op'], {}).get('short_desc', '')))
json.dump(nop, open('C:/gta-scratch/t3/nopvis.json', 'w'))
print('NOP used with non-trivial exe case:', len(nop))
