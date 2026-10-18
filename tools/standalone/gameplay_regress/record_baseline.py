#!/usr/bin/env python3
"""Gameplay regression suite: put numbers MEASURED ON THE ORIGINAL EXE into baseline.json.
usage: python -X utf8 -I record_baseline.py cheats <cheat_probe_exe.py log of an original run of orig/cheats.scn>
       python -X utf8 -I record_baseline.py audio  <audio_stats_exe.py log of an original run of orig/audio.scn>
       python -X utf8 -I record_baseline.py combat <orig_gr_poll.py log of an original run of orig/combat.scn> <inputdrv.log of the same run>
`cheats` stores, for every cheat press of routes/cheats.txt (`probe:cNN_CODE`), the original's state after it (the last sample before the next `timesCheated` increment) incl. the vehicle models that press ADDED (traffic is random,
so only added models are compared). `audio` stores the observed envelope (max collision-list fill, active sounds, channels, looped sounds). `combat` stores when police first appeared / first engaged in the original's chase.
The thresholds (value/why) in baseline.json are hand-written; this tool only fills the "orig" reference fields."""
import json, os, re, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.join(HERE, 'baseline.json')


def kv(s):
    return {m.group(1): m.group(2) for m in re.finditer(r'(\w+)=(\[[^\]]*\]|\S+)', s)}


def load():
    try:
        return json.load(open(BASE, encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def save(b):
    json.dump(b, open(BASE, 'w', encoding='utf-8', newline='\n'), indent=1, sort_keys=False)
    open(BASE, 'a', encoding='utf-8', newline='\n').write('\n')


def models(s):
    return collections.Counter(x for x in s.strip('[]').split(',') if x)


def cheats_dict(path):
    order = re.findall(r'^probe:(c\d+_\w+)$', open(os.path.join(HERE, 'routes', 'cheats.txt'), encoding='utf-8').read(), re.M)
    order = [o for o in order if o != 'start']
    last, first = {}, None
    for ln in open(path, encoding='utf-8', errors='replace'):
        if ln.startswith('t='):
            d = kv(ln)
            n = int(float(d['timesCheated']))
            last[n] = d
            first = n if first is None else min(first, n)
    out, prev = {}, collections.Counter()
    for i, name in enumerate(order, 1):
        d = last.get(first + i)
        if d is None:
            print('no original sample for', name)
            continue
        cur = models(d['veh'])
        added = sorted((cur - prev).elements())
        prev = cur
        out[name] = {k: d[k] for k in ('wl', 'armour', 'money', 'cheated', 'cheatsOn', 'weap', 'weather', 'ts')}
        out[name]['veh_added'] = sorted(set(added))
    return out


def cheats(path):
    out = cheats_dict(path)
    b = load()
    b.setdefault('cheats', {})['value'] = out
    b['cheats']['why'] = 'state of the ORIGINAL exe after each cheat press of routes/cheats.txt (recorded %s from %s); traffic is random, so only the vehicle models of the spawn cheats (Rhino, Trashmaster) are compared; hp / hour / pveh are not compared (the AI and the clock move); armour is not compared after the Village People cheat (peds shoot rockets at CJ)' % (os.popen('date +%F').read().strip(), os.path.basename(path))
    save(b)
    print(len(out), 'cheat states recorded')


def audio(path):
    rows = []
    for ln in open(path, encoding='utf-8', errors='replace'):
        m = re.search(r'coll=(\d+)\(one (\d+) loop (\d+) n=(\d+)\) act=(\d+).*?chUsed=(\d+)/\d+ want=(\d+).*?loop=(\d+)\(virt', ln)
        if m:
            rows.append([int(x) for x in m.groups()])
    o = {'coll_used_max': max(r[0] for r in rows), 'coll_oneshot_max': max(r[1] for r in rows), 'coll_loop_end': rows[-1][2], 'act_max': max(r[4] for r in rows), 'ch_max': max(r[5] for r in rows), 'want_max': max(r[6] for r in rows),
         'loop_snd_max': max(r[7] for r in rows), 'samples': len(rows)}
    b = load()
    b.setdefault('audio', {})['orig_run'] = o
    save(b)
    print('audio original envelope:', o)


def combat_numbers(poll, drv):
    """seconds after the wanted level appeared: first law vehicle / cop near, first sign of engagement (CJ hurt or cops on foot or busted)"""
    marks = {}
    for ln in open(drv, encoding='utf-8', errors='replace'):
        m = re.match(r'\[(\w+)\] qpc=([\d.]+)', ln)
        if m:
            marks[m.group(1)] = float(m.group(2))
    t_cheat = marks.get('chase_cheat') or marks.get('chase_0')
    t0 = first_law = first_eng = base_hp = None
    for ln in open(poll, encoding='utf-8', errors='replace'):
        if not ln.startswith('[gr-o]'):
            continue
        d = kv(ln)
        t = float(d['qpc'])
        if t < t_cheat:
            continue
        if t0 is None and d['wl'] != '0':
            t0 = t
        if t0 is None:
            continue
        hp = int(d['hp']) + int(d['arm'])
        base_hp = hp if base_hp is None else base_hp
        s_ = round((t - t0) / 1000.0)
        if first_law is None and (int(d['law100']) >= 1 or int(d['cops']) >= 1):
            first_law = s_
        if first_eng is None and (int(d['copsFoot']) >= 1 or hp < base_hp - 1 or d['pst'] in ('63', '49')):
            first_eng = s_
    return first_law, first_eng


def combat(poll, drv):
    first_law, first_eng = combat_numbers(poll, drv)
    b = load()
    c = b.setdefault('combat', {})
    c.setdefault('law_arrive_s', {})['orig'] = '%s s after the wanted level appeared' % first_law
    c.setdefault('engage_s', {})['orig'] = '%s s after the wanted level appeared (first sign)' % first_eng
    save(b)
    print('combat original: police near after %s s, first engagement after %s s' % (first_law, first_eng))


if __name__ == '__main__':
    {'cheats': lambda: cheats(sys.argv[2]), 'audio': lambda: audio(sys.argv[2]), 'combat': lambda: combat(sys.argv[2], sys.argv[3])}[sys.argv[1]]()
