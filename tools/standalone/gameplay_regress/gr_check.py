#!/usr/bin/env python3
"""Gameplay regression suite: checker. Reads the logs one scenario run left in <out>/<scenario>/ (standalone.log with `[gr]` snapshot lines, `[mark]`, `[until]`, `[gr-cmd]`, `[probe]` lines, audio_stats.txt,
the original-exe leg in <out>/<scenario>/orig/) and prints one line per check:
    PASS|FAIL|KNOWN|SKIP|SETUP  <scenario>.<check>   <measured>   (<expected / tolerance>)
KNOWN = the check fails but is listed in known.tsv (an open playtest item): shown, not counted. SETUP = the scenario could not reach its precondition (gate timeout, game died early): the runner retries it once.
Thresholds live in baseline.json (each with a "why"); with --live-orig the reference is the original-exe leg of THIS run instead of the recorded numbers.

usage: gr_check.py <scenario> <out-dir> [--live-orig] [--record]      -> exit 0 (no FAIL), 1 (FAIL), 3 (SETUP failure)
       gr_check.py --summary <out-dir>                                  -> table of all <scenario>.result files, exit 1 when any FAIL
"""
import glob, json, math, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ST = {'PED_ARREST_PLAYER': 49, 'PED_DRIVING': 50, 'PED_DIE': 54, 'PED_DEAD': 55, 'PED_ARRESTED': 63}   # ePedState (source/game_sa/Enums/ePedState.h)


# ----------------------------------------------------------------------------------------------- parsing
def read(path):
    try:
        return open(path, encoding='utf-8', errors='replace').read().splitlines()
    except OSError:
        return []


def kv(s):
    d = {}
    for m in re.finditer(r'(\w+)=(\S*)', s):
        d[m.group(1)] = m.group(2)
    return d


def num(d, k, default=None):
    try:
        return float(d[k])
    except (KeyError, ValueError, TypeError):
        return default


class Run:
    def __init__(self, od):
        self.od = od
        self.sl = read(os.path.join(od, 'standalone.log'))
        self.gl = read(os.path.join(od, 'log.log'))
        self.status = (read(os.path.join(od, 'status')) or ['missing 0'])[0].split()
        self.snaps = {}       # 'name' -> merged dict of the A..D parts (the LAST snapshot of that name)
        self.order = []
        self.marks = {}       # name -> (script ms, tick)
        self.gates = []       # (cond, ok?, ms)
        self.cmds = []        # [gr-cmd] / [cmd] lines
        self.cmds_after = []  # (name of the last snapshot taken before the command, line): lets a check look at the commands of ONE route step only (COMBAT-FLAKY)
        self.probes = []      # (name, dict)
        self.hurt = []        # (number of probes logged so far, weapon, died?) per [pc-dmg] event in which an ENTITY lowered the player's armour / killed him (NOTSA_STANDALONE_PROBE=1 runs)
        last_snap = None
        for ln in self.sl:
            m = re.search(r'\[gr\] (\S+?)#(\d+) ([A-D]) (.*)$', ln)
            if m:
                nm = m.group(1)
                last_snap = nm
                if nm not in self.snaps:
                    self.snaps[nm] = {}
                    self.order.append(nm)
                self.snaps[nm].update(kv(m.group(4)))
                continue
            m = re.search(r'\[mark\] (\S+) at (\d+) ms t=(\d+)', ln)
            if m:
                self.marks[m.group(1)] = (int(m.group(2)), int(m.group(3)))
                continue
            m = re.search(r'\[until\] (\S+) (ok|TIMEOUT) after (\d+) ms', ln)
            if m:
                self.gates.append((m.group(1), m.group(2) == 'ok', int(m.group(3))))
                continue
            if '[gr-cmd]' in ln or '[cmd]' in ln or '[vmode]' in ln or '[kill]' in ln or '[mission]' in ln:
                self.cmds.append(ln)
                self.cmds_after.append((last_snap, ln))
                continue
            m = re.search(r'\[pc-dmg\] .* player hp (-?\d+) -> (-?\d+) armour (-?\d+) -> (-?\d+) lastDamager=(\w+) weap=(-?\d+)', ln)
            if m:
                hp0, hp1, a0, a1, who, wp = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5), 16), int(m.group(6))
                if who and ((a0 >= 0 and a1 < a0) or (hp0 > 0 and hp1 <= 0)):
                    self.hurt.append((len(self.probes), wp, hp0 > 0 and hp1 <= 0))
                continue
            m = re.search(r'\[probe\] (c\d+_\w+) (.*)$', ln)
            if m:
                self.probes.append((m.group(1), kv(m.group(2))))

    def snap(self, name):
        return self.snaps.get(name)

    @property
    def quit_ok(self):
        return any('[gr] quit requested' in l for l in self.sl)

    def crash_lines(self):
        pat = re.compile(r'UNREACHABLE|Unhandled|Exception Code|Exception Address|CALL STACK|0xE0AB|ACCESS_VIOLATION|FATAL|\[critical\]|has no ported handler|assertion failed|Assertion failed')
        return [l.strip()[:160] for l in self.sl + self.gl if pat.search(l)]


# ----------------------------------------------------------------------------------------------- result handling
class Results:
    def __init__(self, scen, known, record):
        self.scen, self.known, self.rows, self.measured = scen, known, [], {}
        self.record = record

    def add(self, check, ok, value, expect='', setup=False):
        key = '%s.%s' % (self.scen, check)
        if setup and not ok:
            st = 'SETUP'
        elif ok is None:
            st = 'SKIP'
        elif ok:
            st = 'PASS'
        elif key in self.known:
            st = 'KNOWN'
        else:
            st = 'FAIL'
        self.rows.append((st, key, str(value), expect + ((' ; known: ' + self.known[key]) if st == 'KNOWN' else '')))
        self.measured[key] = value

    def out(self):
        for st, key, val, exp in self.rows:
            print('%-6s %-34s %s%s' % (st, key, val, ('   (%s)' % exp) if exp else ''))

    def code(self):
        sts = [r[0] for r in self.rows]
        if 'SETUP' in sts:
            return 3
        return 1 if 'FAIL' in sts else 0


def load_json(name):
    try:
        return json.load(open(os.path.join(HERE, name), encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def load_known():
    k = {}
    for ln in read(os.path.join(HERE, 'known.tsv')):
        if ln.strip() and not ln.startswith('#'):
            p = ln.split('\t')
            if len(p) >= 2:
                k[p[0].strip()] = p[1].strip()
    return k


def val(B, key, default=None):
    """threshold `key` of a baseline.json group: {"value": x, "why": ...}"""
    return B.get(key, {}).get('value', default)


# ----------------------------------------------------------------------------------------------- generic checks
def generic(run, R, need_control=True, free=False, free_blocks=True):
    """run ended normally, no crash signature, the shared header reached the player's control. Returns False when the set-up failed."""
    crash = run.crash_lines()
    setup_ok = True
    if need_control and 'control' not in run.marks:
        gate = [g for g in run.gates if not g[1]]
        R.add('setup', False, 'header never reached control (last mark %s, gate timeouts %s, status %s)' % (list(run.marks)[-1:] or '-', gate or '-', run.status), 'new game -> control', setup=True)
        setup_ok = False
    if crash:
        frames = [re.sub(r'^.*?\]: ', '', l)[-60:] for l in run.gl if 'gta_reversed!' in l][:3]
        R.add('no_crash', False, '%d crash/assert lines, first: %s%s' % (len(crash), crash[0], (' | top frames: ' + ' <- '.join(frames)) if frames else ''), '0')
    else:
        R.add('no_crash', True, '0 crash/assert lines')
    if setup_ok and free:
        f = run.snap('free_ready')
        ok = bool(f) and num(f, 'mission') == 0 and num(f, 'ctl') == 1 and num(f, 'invh') == 0
        R.add('free_start', ok, 'mission=%s ctl=%s invehicle=%s after mounting/dismounting the tutorial bike' % (f and f.get('mission'), f and f.get('ctl'), f and f.get('invh')), 'CJ free on foot, opening mission over', setup=True)
        setup_ok = ok or not free_blocks   # (combat: the deterministic sections run anyway, so that a broken start does not hide what it was meant to catch)
    if setup_ok:
        R.add('ran_to_end', run.quit_ok and run.status[0] == 'ok', 'status=%s quit_requested=%s' % (' '.join(run.status), run.quit_ok), 'route finished and left the game loop by itself')
    return setup_ok


# ----------------------------------------------------------------------------------------------- scenarios
def hpar(s):
    return (num(s, 'hp', 0) or 0) + (num(s, 'arm', 0) or 0)


def chk_combat(run, R, B, orig):
    if not generic(run, R, free=True, free_blocks=False):
        return
    # ---- A. foot cop
    sp = [l for l in run.cmds if 'spawncop' in l]
    a, b = run.snap('cop_alive'), run.snap('cop_shot')
    if a and b:
        armed = bool(sp) and re.search(r'weapons \[\d', sp[0]) is not None
        R.add('foot.cop_spawned_armed', armed, sp[0].split('[gr-cmd] ')[-1][:70] if sp else 'no spawncop line', 'an armed cop (LAPD model 280, ped type COP)', setup=True)
        R.add('foot.cop_dies', num(b, 'cdead') == 1, 'cop hp=%s state=%s dead=%s' % (b.get('chp'), b.get('cst'), b.get('cdead')), 'lethal damage event -> DIE/DEAD within 4.3 s')
        s0 = run.snap('cop_shot_0')
        dp = int((num(s0, 'pnew', 0) or 0) + (num(b, 'pnew', 0) or 0))
        R.add('foot.weapon_pickup', dp >= val(B, 'foot_pickups_min', 1), 'new pickups after the kill: %d (active %s -> %s)' % (dp, a.get('pickups'), b.get('pickups')), '>= %s (exe CreateDeadPedWeaponPickups: a dead armed cop on foot drops its weapon)' % val(B, 'foot_pickups_min', 1))
    else:
        R.add('foot.snaps', False, 'snapshots cop_alive/cop_shot missing (last mark %s)' % (list(run.marks)[-1:]), '', setup=True)
    # ---- B. player hit
    p0, ha, hb = run.snap('pre_hit'), run.snap('hit_a'), run.snap('hit_b')
    if p0 and ha and hb:
        drop = hpar(p0) - min(hpar(ha), hpar(hb))
        R.add('hit.damage_event', drop >= val(B, 'hit_drop_min', 2), 'health+armour %d -> %d/%d (drop %d)' % (hpar(p0), hpar(ha), hpar(hb), drop), '>= %s for a 10-damage pistol damage event (the exe scales it down for the player)' % val(B, 'hit_drop_min', 2))
    # ---- C. police car + driver
    cs, c2 = run.snap('car_spawned'), run.snap('car_shot_b')
    if cs and c2:
        R.add('car.driver_spawned', num(cs, 'veh') == 1 and num(cs, 'drv') == 1 and num(cs, 'dhp', 0) > 0, 'veh=%s drv=%s dhp=%s dst=%s tasks=%s' % (cs.get('veh'), cs.get('drv'), cs.get('dhp'), cs.get('dst'), cs.get('dtasks')), 'a police car with a living cop driver', setup=True)
        # COMBAT-FLAKY: a dead driver's corpse may already be removed by car_shot_b (the pool slot is gone: dhp=-1, ddead=-1), like the corpses of the kill step: look at both samples (a living driver shows ddead=0 in both)
        ca = run.snap('car_shot_a') or c2
        dd = 1 if (num(ca, 'ddead') == 1 or num(c2, 'ddead') == 1) else 0
        sd = ca if num(ca, 'ddead') == 1 and num(c2, 'ddead') != 1 else c2
        R.add('car.driver_dies', dd == 1, 'driver hp=%s state=%s (%s) tasks=%s' % (sd.get('dhp'), sd.get('dst'), 'dying/dead' if dd == 1 else 'ALIVE: events never reach the car occupant', sd.get('dtasks')), 'lethal damage event on a driver -> DIE/DEAD within 4 s (CTaskSimpleCarDrive::MakeAbortable must say yes)')
    # ---- D. chase
    c0 = run.snap('chase_0')
    if c0:
        names = [n for n in run.order if re.fullmatch(r'chase_[0-9]+', n)]
        R.add('chase.wanted_set', num(c0, 'wl', 0) >= 2, 'wanted %s after TURNUPTHEHEAT' % c0.get('wl'), '>= 2')
        t_of = lambda n: int(n.split('_')[1])
        law_t = next((t_of(n) for n in names if num(run.snaps[n], 'law100', 0) >= 1 or num(run.snaps[n], 'cops', 0) >= 1), None)
        lim = val(B, 'law_arrive_s', 40)
        R.add('chase.police_arrive', law_t is not None and law_t <= lim, 'first cop/law car near at %ss' % law_t, '<= %ss (original: %s)' % (lim, orig.get('law_arrive_s', B.get('law_arrive_s', {}).get('orig'))))
        base = hpar(c0)
        eng_t = None
        for n in names:
            s = run.snaps[n]
            if num(s, 'copsFoot', 0) >= 1 or num(s, 'copsEngaged', 0) >= 1 or hpar(s) < base - 1 or num(s, 'pst', 0) in (ST['PED_ARRESTED'], ST['PED_ARREST_PLAYER']) or num(s, 'wl', 9) == 0:
                eng_t = t_of(n)
                break
        lim = val(B, 'engage_s', 70)
        pre, last = run.snap('chase_pre') or c0, run.snaps[names[-1]] if names else c0
        hurt = min(hpar(run.snaps[n]) for n in names) < hpar(pre) - 1
        busted = any(num(run.snaps[n], 'pst', 0) in (ST['PED_ARRESTED'], ST['PED_ARREST_PLAYER']) or num(run.snaps[n], 'wl', 9) == 0 for n in names)
        hf = (num(last, 'hitevents', 0) or 0) - (num(pre, 'hitevents', 0) or 0)
        R.add('chase.player_shot_at', hurt or busted, 'health+armour min %d (start %d), busted/cleared: %s, hit/fall reaction tasks started: %d (info: CJ only flinches from bigger hits)' % (min(hpar(run.snaps[n]) for n in names), hpar(pre), busted, hf), 'the police hurt CJ (or arrest him) within 70 s (U7: shots did no damage); at Idlewood the original cops shoot from the first seconds')
        R.add('chase.police_engage', eng_t is not None and eng_t <= lim, 'first sign (cop on foot / pursuit task / player damaged / busted) at %ss' % eng_t, 'within %ss (original: %s; the chase has randomness, so any one sign counts)' % (lim, orig.get('engage_s', B.get('engage_s', {}).get('orig'))))
    kb, k6 = run.snap('kill_before'), run.snap('kill_6')
    # only the shots of the kill step itself: when CJ was wasted / busted in the chase the cops are gone at that step, and the later 'shoot:6:200' clean-up shot (a cop that arrived afterwards) must not count as "a cop was shot here" (COMBAT-FLAKY: 3 false FAILs)
    shot = [l for (sn, l) in run.cmds_after if 'shot ped' in l and sn == 'kill_before']
    if kb and k6:
        k2 = run.snap('kill_2') or k6   # (corpses vanish after a few seconds: look at both samples)
        if not shot:
            R.add('chase.no_zombie_cops', None, 'no cop of type 6 within 100 m at the kill step', 'skipped')
        else:
            dead = max(num(k2, 'copsDead', 0), num(k6, 'copsDead', 0))
            zomb = max(num(k2, 'copsDeadAlive', 0), num(k6, 'copsDeadAlive', 0))
            R.add('chase.no_zombie_cops', zomb == 0 and dead >= 1, '%d cops shot; afterwards dead=%d dead-but-not-dying=%d' % (len(shot), dead, zomb), 'every cop at 0 hp dies (no hp-0 cops left driving / standing)')


def chk_audio(run, R, B, orig):
    if not generic(run, R, free=True):
        return
    tank = run.snap('in_car')
    R.add('car.entered', bool(tank) and num(tank, 'invh') == 1 and num(tank, 'vmodel') in (505, 410), 'invh=%s model=%s' % (tank and tank.get('invh'), tank and tank.get('vmodel')), 'CJ sits in a car (the Rancher 505, or one of the parked Mananas 410)', setup=True)
    rows = []
    for ln in read(os.path.join(run.od, 'audio_stats.txt')):
        m = re.search(r'coll=(\d+)\(one (\d+) loop (\d+) n=(\d+)(?: add=(\d+))?\) act=(\d+).*?phys=(\d+).*?chUsed=(\d+)/(\d+) want=(\d+)\S*.*?stopReq=(\d+) loop=(\d+)\(virt (\d+)\)(?:.*?req=(\d+) refused=(\d+) collFull=(\d+))?', ln)
        if m:
            rows.append([int(x) if x else 0 for x in m.groups()])
    if len(rows) < 30:
        R.add('stats', False, '%d audio-stat samples' % len(rows), '>= 30 (NOTSA_AUDIO_STATS)', setup=True)
        return
    mx = lambda i: max(r[i] for r in rows)
    end = rows[-1]
    R.add('scene_has_contacts', end[4] >= 6, '%d collision-list additions in the scene' % end[4], '>= 6 (a scene without contacts cannot show a leak)', setup=True)
    o = orig
    ref = lambda k: o.get(k, B.get(k, {}).get('orig'))
    R.add('coll_list_max', mx(0) <= val(B, 'coll_used_max'), 'collision list used: max %d, end %d (adds %d)' % (mx(0), end[0], end[4]), '<= %s (original max %s)' % (val(B, 'coll_used_max'), ref('coll_used_max')))
    R.add('coll_list_released', end[1] <= val(B, 'coll_oneshot_end_max') and end[0] <= val(B, 'coll_used_end_max'),
          'at the end (25 s idle): used %d, one-shot %d, looping %d' % (end[0], end[1], end[2]), 'used <= %s, one-shot <= %s (one-shot entries are released by UpdateParameters 0x4DA370)' % (val(B, 'coll_used_end_max'), val(B, 'coll_oneshot_end_max')))
    R.add('active_sounds', mx(5) <= val(B, 'act_max'), 'active sounds max %d (table 300), end %d' % (mx(5), end[5]), '<= %s (original max %s)' % (val(B, 'act_max'), ref('act_max')))
    R.add('channels', mx(7) <= min(end[8], val(B, 'ch_max')), 'hardware channels in use max %d of %d' % (mx(7), end[8]), '<= %s (original max %s)' % (val(B, 'ch_max'), ref('ch_max')))
    R.add('starvation', mx(9) <= val(B, 'want_max') and mx(14) == 0 and mx(15) == 0, 'audible sounds without a channel max %d; refused requests %d; list-full events %d' % (mx(9), mx(14), mx(15)), 'want <= %s, refused 0, collFull 0 (original 0)' % val(B, 'want_max'))
    R.add('no_stuck_loops', mx(11) <= val(B, 'loop_snd_max') and end[2] <= val(B, 'coll_loop_end_max'), 'looped sounds max %d (end %d), looping collision entries at the end %d' % (mx(11), end[11], end[2]), 'sounds <= %s (original max %s), collision loops at the end <= %s (original 3)' % (val(B, 'loop_snd_max'), ref('loop_snd_max'), val(B, 'coll_loop_end_max')))


SPAWN_CHEATS = {'c17': '432', 'c24': '408'}   # Rhino, Trashmaster


def chk_cheats(run, R, B, orig):
    if not generic(run, R, free=True):
        return
    base = orig.get('cheats') or B.get('value') or {}
    got = {n: d for n, d in run.probes}
    if len(got) < 10:
        R.add('probes', False, '%d probe lines' % len(got), '>= 10', setup=True)
        return
    keys = ['wl', 'armour', 'money', 'cheated', 'cheatsOn', 'weap', 'weather', 'ts']

    def norm(k, v):
        if k == 'armour':
            return '%.0f' % float(v)
        if k == 'weather':
            return str(int(v.split('/')[0]) & 0xFFFF)
        return v
    bad, n, prev, attacked, died = [], 0, {}, False, False
    why = ''
    order = [x for x, _ in run.probes]
    for name in [x for x in order if x != 'start']:
        attacked = attacked or name.startswith('c40_')     # from the Village People cheat on, peds shoot rockets at CJ: his armour then depends on the fight
        # CHEATS-ARMOUR: a gang ped of the Idlewood start area (Ballas, pistol / fists) that attacks CJ is the game's own population, not a cheat effect: once an entity has lowered CJ's armour (or killed him)
        # in the window of a probe, armour is not comparable from that probe on (his armour only goes up again with the next HESOYAM); a death also takes weapons and money (hospital fee). The cheat's own armour changes (HESOYAM) have no damager.
        for (k, wp, dead) in run.hurt:
            if k <= order.index(name):
                if not attacked or (dead and not died):
                    why = why or '%s on: CJ %s by weapon %d (pc-dmg), armour%s not compared' % (name, 'killed' if dead else 'hurt', wp, ' / weapons / money' if dead else '')
                attacked = True
                died = died or dead
        ref = base.get(name)
        d = got[name]
        models = {}
        for x in d.get('veh', '').strip('[]').split(','):
            if x:
                models[x] = models.get(x, 0) + 1
        added = sorted(k for k in models if models[k] > prev.get(k, 0))
        prev = models
        if not ref:
            continue
        n += 1
        diffs = ['%s %s != %s' % (k, norm(k, d.get(k, '?')), norm(k, ref.get(k, '?'))) for k in keys if k in ref and not (k == 'armour' and attacked) and not (died and k in ('weap', 'money')) and norm(k, d.get(k, '?')) != norm(k, ref.get(k, '?'))]
        spawn = SPAWN_CHEATS.get(name.split('_')[0])    # only the cheats that spawn a vehicle: that model must appear among the vehicles added (traffic comes and goes at random)
        if spawn and (spawn not in added) != (spawn not in (ref.get('veh_added') or [])):
            diffs.append('spawned model %s: ours added %s, original added %s' % (spawn, spawn in added, spawn in (ref.get('veh_added') or [])))
        if diffs:
            bad.append('%s: %s' % (name, '; '.join(diffs)))
    R.add('state_matches_original', not bad and n > 0, '%d cheats compared, %d differ%s%s' % (n, len(bad), (': ' + ' | '.join(bad[:3])) if bad else '', ('; ' + why) if why else ''), 'wanted / armour / money / cheat flags / weapons / weather / time scale / spawned models equal to the original exe after the same cheat')


def chk_missions(run, R, B, orig):
    if not generic(run, R):
        return
    s0 = run.snap('m_start')
    R.add('first_mission_started', bool(s0) and num(s0, 'mission') == 1, 'on a mission after the new game: %s' % (s0 and s0.get('mission')), '1 (the opening mission is running when CJ gets control)')
    bs = run.snap('bike_spawned')
    R.add('vehicle.controls_back', bool(bs) and num(bs, 'ctl') == 1, 'controls after the busted flow: ctl=%s' % (bs and bs.get('ctl')), 'CJ can act again when the bike is spawned', setup=True)
    for nm, key, exp_in, label in (('bike_in_1', 'bike.enter_return', 1, 'Return mounts the BMX'), ('bike_out', 'bike.exit_return', 0, 'Return dismounts'), ('bike_in_2', 'bike.reenter_return', 1, 'Return mounts the BMX again'), ('car_in', 'car.enter_return', 1, 'Return enters a car')):
        s = run.snap(nm)
        if s is None:
            R.add(key, False, 'snapshot %s missing' % nm, label)
            continue
        ok = num(s, 'invh') == exp_in and (exp_in == 0 or num(s, 'vmodel') in (481, 410))
        R.add(key, ok, 'invehicle=%s model=%s' % (s.get('invh'), s.get('vmodel')), label)
    pa, ab15 = run.snap('pre_arrest'), run.snap('after_busted_15')
    if pa and ab15:
        R.add('arrest.during_mission', num(pa, 'mission') == 1, 'mission flag before the arrest: %s' % pa.get('mission'), '1 (the opening mission is still running)', setup=True)
        gate = [g for g in run.gates if g[0] == 'control']
        regained = len(gate) >= 2 and gate[-1][1]
        R.add('arrest.game_goes_on', regained and 'busted_done' in run.marks, 'busted flow ended, control regained: %s (gate %s)' % (regained, gate[-1] if gate else None), 'after arrest:0 the busted flow completes and CJ can move again (the old ResetIP read stack[1]: AV in ProcessOneCommand)')
        R.add('arrest.mission_cleaned_up', num(ab15, 'mission') == 0 and num(ab15, 'fail') == 0 and num(ab15, 'pst') != ST['PED_ARRESTED'], 'mission flag %s -> %s, FailCurrentMission=%s, ped state %s' % (pa.get('mission'), ab15.get('mission'), ab15.get('fail'), ab15.get('pst')), 'mission 1 -> 0 (the script resumed at the mission cleanup after the first GOSUB, exe 0x469F00 ResetIP)')


def chk_arrest0(run, R, B, orig):
    if not generic(run, R):
        return
    for tag, pre, post, start, want, label in (
            ('busted_fee_100', 'pre_busted_pos', 'after_busted_pos', 1000, 900, 'arrested at 0 stars: the fee switch falls through to 100 (exe 0x4431CA `mov ecx, 100`)'),
            ('busted_negative_money_kept', 'pre_busted_neg', 'after_busted_neg', -50, -50, 'busted with money <= 0: the exe skips the subtraction (`cmp [money], 0 ; jle`, 0x4431F1), the money stays'),
            ('wasted_fee_100', 'pre_wasted_pos', 'after_wasted_pos', 1000, 900, 'wasted: fee 100 (0x442E3A)'),
            ('wasted_negative_money_kept', 'pre_wasted_neg', 'after_wasted_neg', -50, -50, 'wasted with money <= 0: money stays (0x442E38 `jle`)')):
        a, b = run.snap(pre), run.snap(post)
        if not a or not b:
            R.add(tag, False, 'snapshot %s / %s missing' % (pre, post), label)
            continue
        if tag.startswith('busted_fee'):
            R.add('wanted_level_0', num(a, 'wl') == 0, 'wanted level before the arrest: %s' % a.get('wl'), '0 stars', setup=True)
        R.add(tag, num(a, 'money') == start and num(b, 'money') == want, 'money %s -> %s' % (a.get('money'), b.get('money')), '%d -> %d: %s' % (start, want, label))


def chk_vmode(run, R, B, orig):
    if not generic(run, R):
        return
    seen = 0
    for nm, want in (('vm_0', (960, 600)), ('vm_1', (1600, 900)), ('vm_2', (2560, 1440)), ('vm_3', (960, 600))):
        s = run.snap(nm)
        if not s:
            R.add(nm, False, 'snapshot missing', '')
            continue
        m, f, c = (re.match(r'(\d+)x(\d+)', s.get(k, '')) for k in ('cam', 'front', 'client'))
        if not (m and f and c):
            R.add(nm, False, 'unparsable %s' % s, '')
            continue
        cam, fr, cl = [tuple(int(x) for x in g.groups()) for g in (m, f, c)]
        mode = (int(s['maxw']), int(s['maxh']))
        if mode != want:
            R.add(nm + '.mode', None, 'mode %dx%d (wanted %dx%d: not offered by this display)' % (mode + want), 'skipped')
            continue
        seen += 1
        if nm == 'vm_1' and num(s, 'hour', 0) < 10:
            R.add('heat_haze_hour', None, 'game hour %s (< 10: the heat haze post effect is not active)' % s.get('hour'), 'skipped')
        R.add(nm + '.front_covers_camera', fr[0] >= cam[0] and fr[1] >= cam[1], 'mode %dx%d camera raster %dx%d post-effects front buffer %dx%d' % (mode + cam + fr), 'front >= camera raster in both dimensions (a smaller one fails the StretchRect with D3DERR_INVALIDCALL every frame -> dark squares)')
        R.add(nm + '.camera_is_mode', cam == mode and cl == mode, 'camera %dx%d client %dx%d mode %dx%d' % (cam + cl + mode), 'camera raster and window client = the video mode (the exe\'s WM_SIZE body ran)')
    if seen < 2:
        R.add('modes_exercised', None, 'only %d snapshots in the wanted modes' % seen, 'skipped')


def chk_mouse(run, R, B, orig):
    if not generic(run, R):
        return
    s0 = run.snap('ms_0')
    R.add('relative_mode_on', bool(s0) and num(s0, 'relmouse') == 1, 'SDL relative mouse mode in game: %s' % (s0 and s0.get('relmouse')), '1 (hidden cursor, unbounded deltas; off in the user\'s playtest build)')
    c = [num(run.snap(n) or {}, 'cam10') for n in ('ms_0', 'ms_1', 'ms_2', 'ms_3')]
    if None in c:
        R.add('sweeps', False, 'snapshots missing', '', setup=True)
        return
    d = [(c[i + 1] - c[i]) / 10.0 for i in range(3)]
    R.add('sweep1_rotates', abs(d[0]) >= val(B, 'sweep_min_deg', 90), 'camera turned %.0f deg in sweep 1' % d[0], '>= %s deg' % val(B, 'sweep_min_deg', 90))
    r = val(B, 'sweep_repeat_ratio', 0.5)
    R.add('sweep2_not_stalled', abs(d[1]) >= r * abs(d[0]) and abs(d[2]) >= r * abs(d[0]), 'sweeps 2/3 turned %.0f / %.0f deg (sweep 1: %.0f)' % (d[1], d[2], d[0]), '>= %s x sweep 1 (the cursor hitting the screen edge stops the camera)' % r)
    R.add('past_360', abs(c[3] - c[0]) / 10.0 >= 360 and all(x * d[0] > 0 for x in d), 'total %.0f deg, same direction %s' % ((c[3] - c[0]) / 10.0, all(x * d[0] > 0 for x in d)), '>= 360 deg in one direction')


def chk_firststart(run, R, B, orig):
    s = run.snap('fs_menu2') or run.snap('fs_menu')
    if not s:
        R.add('setup', False, 'no snapshot (status %s, marks %s)' % (run.status, list(run.marks)), '', setup=True)
        return
    generic(run, R, need_control=False)
    m = re.match(r'(\d+)x(\d+)', s.get('client', ''))
    cl = (int(m.group(1)), int(m.group(2))) if m else None
    mode = (int(s['maxw']), int(s['maxh']))
    R.add('window_is_mode_size', cl == mode, 'window client %s, video mode %dx%d' % (cl, mode[0], mode[1]), 'equal (the user saw the game in a part of the screen)')
    R.add('default_mode_800x600', mode == (800, 600), 'default mode %dx%d' % mode, '800x600 (exe CheckDefaultVideoModeSupported: first 800x600x32 exclusive mode; the original does the same, PT-RENDER orig_800)')


def chk_interior(run, R, B, orig):
    if not generic(run, R, free=True):
        return
    S = [run.snap(n) for n in ('int_t4', 'int_t10', 'int_t20', 'int_t30')]
    S = [s for s in S if s]
    if len(S) < 4:
        R.add('snaps', False, 'snapshots missing (%d of 4)' % len(S), '', setup=True)
        return
    R.add('entered', any(num(s, 'area', 0) != 0 for s in S[:2]), 'CGame::currArea %s' % [s.get('area') for s in S], 'CJ is inside the interior (area != 0) 4-10 s after the teleport to the entrance')
    # only the snapshots taken INSIDE the interior (area != 0): the later ones (busted / wasted, CJ back on the street) see the exterior police, which streams again and stands on the street 2+ m
    # below / above whatever the interior check is about (REGRESS-FLAKES: that made cops_on_floor fail ~1 run in 3). (Peds carry no usable area of their own here: the cops of the interior show up in cops40 with area code 0, so the filter is the area CJ is in.)
    Sin = [s for s in S if num(s, 'area', 0) != 0]
    R.add('cops_present', bool(Sin) and max(num(s, 'cops40', 0) for s in Sin) >= 1, 'cops within 40 m (snapshots inside the interior): %s of all %s' % ([s.get('cops40') for s in Sin], [s.get('cops40') for s in S]), '>= 1 (the station is populated by PopulateInterior)')
    dz_of = lambda ss: [int(x) for s in ss for x in s.get('copdz', '-').split(',') if x not in ('', '-')]
    dz_all = dz_of(Sin)
    # LSPD-POP: the original exe also keeps cops on another level of the station (its poller: +14 dm for the whole 15 s, -12 / -26 dm too: nodes of the upper / lower walkways), so a vertical-offset limit is not a property of the exe.
    # U15 (cops stacked on top of CJ) is the cops standing ABOVE him at the same XY: `copstk` = living cops within 40 m, horizontal distance < 1.5 m and > 0.5 m higher.
    lim = val(B, 'cop_stack_max', 0)
    stk = [int(num(s, 'copstk', 0)) for s in Sin]
    R.add('cops_on_floor', bool(Sin) and max(stk) <= lim, 'cops standing above CJ (xy < 1.5 m, > 0.5 m higher), snapshots inside the interior: %s; vertical offsets of the cops (dm, info): %s' % (stk, dz_all[:12] or '-'), '<= %s (U15: the playtest saw them spawn on top of CJ and stay there; cops on other levels of the station are the exe own placement, LSPD-POP)' % lim)
    base = hpar(S[0])
    eng = [s for s in S if num(s, 'copsEngaged', 0) >= 1 or hpar(s) < base - 1 or num(s, 'pst', 0) in (ST['PED_ARRESTED'], ST['PED_ARREST_PLAYER']) or num(s, 'wl', 9) == 0]
    oi = orig.get('interior')
    if oi is not None:   # the original exe's own leg (orig_gr_poll.py: PED_TYPE_COP peds of CJ's area within 40 m); informational: the original shows NO cop-type ped inside the station (REGRESS-FLAKES.md)
        R.add('orig_reference', oi['entered'], 'original exe: entered the interior %s, cop-type peds within 40 m inside (max) %d, vertical offsets (dm): %s' % (oi['entered'], oi['cops'], oi['dz'][:12] or '-'), 'info: the original got into the interior (no pass/fail on its cops)')
    R.add('cops_react', bool(eng), 'engaged cops %s, health+armour %s, wanted %s, ped state %s' % ([s.get('copsEngaged') for s in S], [int(hpar(s)) for s in S], [s.get('wl') for s in S], [s.get('pst') for s in S]), 'within 30 s at 3 stars some cop attacks / arrests / the player is hurt or busted')


CHECKS = {'combat': chk_combat, 'audio': chk_audio, 'cheats': chk_cheats, 'missions': chk_missions, 'render_vmode': chk_vmode, 'input_mouse': chk_mouse, 'render_firststart': chk_firststart, 'interior_lspd': chk_interior, 'arrest0': chk_arrest0}


def live_orig(scn, od):
    """numbers of the original-exe leg of THIS run (empty when there was none)"""
    d = os.path.join(od, scn, 'orig')
    o = {}
    if scn == 'audio':
        rows = []
        for ln in read(os.path.join(d, 'audio_stats.txt')):
            m = re.search(r'coll=(\d+)\(one (\d+) loop (\d+) n=(\d+)\) act=(\d+).*?chUsed=(\d+)/\d+ want=(\d+).*?loop=(\d+)\(virt', ln)
            if m:
                rows.append([int(x) for x in m.groups()])
        if rows:
            o = {'coll_used_max': max(r[0] for r in rows), 'act_max': max(r[4] for r in rows), 'ch_max': max(r[5] for r in rows), 'loop_snd_max': max(r[7] for r in rows), 'want_max': max(r[6] for r in rows)}
    if scn == 'cheats':
        try:
            sys.path.insert(0, HERE); import record_baseline
            if os.path.exists(os.path.join(d, 'orig_probe.log')):
                o['cheats'] = record_baseline.cheats_dict(os.path.join(d, 'orig_probe.log'))
        except Exception as e:   # a broken original leg must not hide our own result
            print('# original leg unusable:', e)
    if scn == 'combat':
        try:
            sys.path.insert(0, HERE); import record_baseline
            law, eng = record_baseline.combat_numbers(os.path.join(d, 'poll.txt'), os.path.join(d, 'inputdrv.log'))
            o = {'law_arrive_s': '%s s (this run)' % law, 'engage_s': '%s s (this run)' % eng}
        except Exception as e:
            print('# original leg unusable:', e)
    if scn == 'interior_lspd':
        o['interior'] = {'entered': False, 'cops': 0, 'dz': []}
        for ln in read(os.path.join(d, 'poll.txt')):
            m = re.search(r'area=(\d+) cops40=(\d+) copdz=(\S*)', ln)
            if m and int(m.group(1)) != 0:
                o['interior']['entered'] = True
                o['interior']['cops'] = max(o['interior']['cops'], int(m.group(2)))
                o['interior']['dz'] += [int(x) for x in m.group(3).split(',') if x not in ('', '-')]
    return o


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == '--summary':
        od = sys.argv[2]
        tot = {}
        print('=' * 100)
        for f in sorted(glob.glob(os.path.join(od, '*.result'))):
            rows = [l.rstrip('\n') for l in read(f) if re.match(r'(PASS|FAIL|KNOWN|SKIP|SETUP)\s', l)]
            scn = os.path.basename(f)[:-7]
            if scn.endswith('.attempt1') and os.path.exists(os.path.join(od, scn[:-9] + '.result')):
                # REGRESS-SETUP: a first attempt whose set-up failed and that gameplay_regress.sh re-ran: the scenario's verdict is the retry's, the failed attempt is info only (not counted)
                setup_rows = [l for l in rows if l.startswith('SETUP')]
                print('%-18s info   first attempt of %s: set-up failed, retried, verdict = the retry%s' % (scn, scn[:-9], ''.join('\n    ' + l[:200] for l in setup_rows[:2])))
                continue
            for l in rows:
                tot[l.split()[0]] = tot.get(l.split()[0], 0) + 1
            bad = [l for l in rows if l.startswith(('FAIL', 'SETUP'))]
            print('%-18s %-5s  %s' % (scn, 'FAIL' if bad else 'ok', ' '.join('%s=%d' % (k, sum(1 for l in rows if l.startswith(k))) for k in ('PASS', 'FAIL', 'KNOWN', 'SKIP', 'SETUP') if any(l.startswith(k) for l in rows))))
            for l in bad:
                print('    ' + l[:200])
        print('-' * 100)
        print('TOTAL ' + ' '.join('%s=%d' % kv_ for kv_ in sorted(tot.items())) + '   =>  ' + ('FAIL' if tot.get('FAIL') or tot.get('SETUP') else 'PASS'))
        print('=' * 100)
        sys.exit(1 if tot.get('FAIL') or tot.get('SETUP') else 0)
    scn, od = sys.argv[1], sys.argv[2]
    live = '--live-orig' in sys.argv
    base = load_json('baseline.json')
    R = Results(scn, load_known(), '--record' in sys.argv)
    B = base.get(scn, {})
    orig = live_orig(scn, od) if live else {}
    CHECKS[scn](Run(os.path.join(od, scn)), R, B, orig)
    R.out()
    if '--record' in sys.argv:
        json.dump(R.measured, open(os.path.join(od, 'measured_%s.json' % scn), 'w'), indent=1)
    sys.exit(R.code())


if __name__ == '__main__':
    main()
