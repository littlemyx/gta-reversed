#!/usr/bin/env python3
"""VT-OVERRIDES: compare the cheat scene of the port (`[probe] c<idx>_<CODE> ...` lines of standalone.log, tools/standalone/cheats_route.txt) with the same scene on the ORIGINAL exe
(tools/standalone/windows/cheat_probe_exe.py log, tools/standalone/windows/cheats_scenes.txt). The n-th typed cheat is matched through `timesCheated` (the original's sample just before the next increment).
usage: python -X utf8 -I tools/standalone/compare_cheat_probes.py <ours standalone.log> <orig_probe.log>
Prints one line per cheat: OK, or the differing fields (the vehicle list is compared as the multiset of models ADDED since the previous cheat, traffic being random).
"""
import re, sys, collections

def parse(line):
    d = {}
    for m in re.finditer(r'(\w+)=(\[[^\]]*\]|[^\s]+)', line):
        d[m.group(1)] = m.group(2)
    return d

ours = []
for ln in open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = re.search(r'\[probe\] (c\d+_\w+) (.*)$', ln)
    if m:
        d = parse(m.group(2)); d['name'] = m.group(1); ours.append(d)
orig = []
for ln in open(sys.argv[2], encoding='utf-8', errors='replace'):
    if ln.startswith('t='):
        d = parse(ln); orig.append(d)
last = {}
for d in orig:
    last[int(float(d['timesCheated']))] = d            # the last sample of each cheat count
def models(s):
    return collections.Counter(int(x) for x in s.strip('[]').split(',') if x)
KEYS = ['wl', 'armour', 'money', 'cheated', 'timesCheated', 'cheatsOn', 'weap', 'pveh', 'weather', 'ts']
prev_o, prev_p = collections.Counter(), collections.Counter()
bad = 0
for d in ours:
    n = int(float(d['timesCheated']))
    o = last.get(n)
    if o is None:
        print('%-26s no original sample for timesCheated=%d' % (d['name'], n)); bad += 1; continue
    diffs = []
    for k in KEYS:
        a, b = d.get(k), o.get(k)
        if k in ('armour',): a, b = '%.0f' % float(a), '%.0f' % float(b)
        if k == 'weather': a, b = a.split('/')[0], b.split('/')[0]
        if a != b: diffs.append('%s ours %s orig %s' % (k, a, b))
    vo, vp = models(d['veh']), models(o['veh'])
    ao, ap = vo - prev_o, vp - prev_p
    prev_o, prev_p = vo, vp
    if set(ao) != set(ap) and (ao or ap):
        diffs.append('added vehicles ours %s orig %s' % (sorted(ao.elements()), sorted(ap.elements())))
    print('%-26s %s' % (d['name'], 'OK' if not diffs else '; '.join(diffs))); bad += bool(diffs)
print(len(ours), 'cheats compared,', bad, 'differ')
