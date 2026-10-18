#!/usr/bin/env python3
"""VT-OVERRIDES: sample the game state of a RUNNING ORIGINAL gta_sa.exe (v1.0 US) from outside, in the terms of the run build's `probe:NAME` injector item
(source/app/platform/SDLWrapper.cpp InjProbe), so a scripted cheat / vehicle / event scene can be compared field by field between the original and the port.

usage: python -X utf8 -I tools/standalone/windows/cheat_probe_exe.py <out-file> [interval-s=0.5] [process=gta_sa.exe] [max-s=3600]
One line is written whenever the state CHANGED (and every 5 s as a heartbeat): `t=<wall s> wl=.. chaos=.. hp=.. armour=.. money=.. cheated=.. timesCheated=.. cheatsOn=[..] weap=[type:ammo,..13 slots]
pveh=<model of the vehicle the player sits in or -1> nveh=.. veh=[sorted model ids of all vehicles] weather=<forced>/<new> ts=<time scale> hour=..`.
Addresses (v1.0 US): Players[0] 0xB7CD98 (ped +0, wanted ptr +4, money +0xB8), CCheat 0x969130 (92 flags) / 0x96918C, int stat cells 0xB78E20 + 4*id (id 137 = times cheated),
vehicle pool 0xB74494 (storage +0, slot flags +4, capacity +8, stride 0xA18), weather 0xC81318 / 0xC8131C, time scale 0xB7CB64, hour 0xB70153.
CPed: health +0x540, armour +0x548, weapons +0x5A0 (13 x 0x1C), vehicle +0x58C, flags dword +0x46C bit 8 (bInVehicle). CWanted: wanted level +0x2C, chaos +0.
"""
import ctypes, ctypes.wintypes as wt, struct, sys, time, subprocess

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.OpenProcess.restype = wt.HANDLE
k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]


def find_pid(name):
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq ' + name, '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
    for ln in out.splitlines():
        p = [x.strip('"') for x in ln.split('","')]
        if len(p) > 1 and p[0].lower() == name.lower():
            return int(p[1])
    return None


def rd(h, addr, n):
    buf = ctypes.create_string_buffer(n)
    got = ctypes.c_size_t(0)
    ok = k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got))
    return buf.raw[:got.value] if ok and got.value == n else None


def u32(h, a):
    b = rd(h, a, 4)
    return struct.unpack('<I', b)[0] if b else None


def sample(h):
    ped = u32(h, 0xB7CD98)
    wptr = u32(h, 0xB7CD9C)
    if not ped or not wptr:
        return None
    w = rd(h, wptr, 0x30)
    p = rd(h, ped, 0x700)
    if w is None or p is None:
        return None
    chaos, = struct.unpack_from('<i', w, 0)
    wl, = struct.unpack_from('<I', w, 0x2C)
    hp, = struct.unpack_from('<f', p, 0x540)
    arm, = struct.unpack_from('<f', p, 0x548)
    weap = ''.join('%d:%d,' % (struct.unpack_from('<I', p, 0x5A0 + i * 0x1C)[0], struct.unpack_from('<I', p, 0x5A0 + i * 0x1C + 0xC)[0]) for i in range(13))
    veh_ptr, = struct.unpack_from('<I', p, 0x58C)
    inveh = (struct.unpack_from('<I', p, 0x46C)[0] >> 8) & 1
    pveh = -1
    if inveh and veh_ptr:
        m = rd(h, veh_ptr + 0x22, 2)
        pveh = struct.unpack('<H', m)[0] if m else -1
    money, = struct.unpack('<i', rd(h, 0xB7CE50, 4))
    flags = rd(h, 0x969130, 92)
    cheats = ''.join('%d,' % i for i, b in enumerate(flags) if b)
    cheated = rd(h, 0x96918C, 1)[0]
    tc, = struct.unpack('<i', rd(h, 0xB78E20 + 4 * 137, 4))
    pool = u32(h, 0xB74494)
    models = []
    if pool:
        hdr = rd(h, pool, 16)
        stor, slots, cap = struct.unpack_from('<III', hdr, 0)
        sl = rd(h, slots, cap)
        for i in range(cap):
            if not (sl[i] & 0x80):
                m = rd(h, stor + i * 0xA18 + 0x22, 2)
                models.append(struct.unpack('<H', m)[0])
    models.sort()
    fw, nw = struct.unpack('<ii', rd(h, 0xC81318, 8))
    ts, = struct.unpack('<f', rd(h, 0xB7CB64, 4))
    hour = rd(h, 0xB70153, 1)[0]
    return ('wl=%d chaos=%d hp=%.1f armour=%.1f money=%d cheated=%d timesCheated=%d cheatsOn=[%s] weap=[%s] pveh=%d nveh=%d veh=[%s] weather=%d/%d ts=%.3f hour=%d' %
            (wl, chaos, hp, arm, money, cheated, tc, cheats, weap, pveh, len(models), ''.join('%d,' % m for m in models), fw, nw, ts, hour))


def main():
    out = open(sys.argv[1], 'w', buffering=1)
    interval = float(sys.argv[2]) if len(sys.argv) > 2 else 0.5
    name = sys.argv[3] if len(sys.argv) > 3 else 'gta_sa.exe'
    tmax = float(sys.argv[4]) if len(sys.argv) > 4 else 3600
    t0 = time.time()
    h = None
    last, lastw = None, 0
    while time.time() - t0 < tmax:
        if not h:
            pid = find_pid(name)
            if not pid:
                time.sleep(1)
                continue
            h = k32.OpenProcess(0x0410, False, pid)
            out.write('# attached pid %d\n' % pid)
        s = sample(h)
        if s is None:
            if find_pid(name) is None:
                out.write('# process gone\n')
                break
            time.sleep(interval)
            continue
        now = time.time()
        if s != last or now - lastw > 5:
            out.write('t=%.1f %s\n' % (now - t0, s))
            last, lastw = s, now
        time.sleep(interval)


if __name__ == '__main__':
    main()
