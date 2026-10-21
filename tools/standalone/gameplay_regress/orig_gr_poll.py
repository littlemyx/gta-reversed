#!/usr/bin/env python3
"""Gameplay regression suite: sample the combat / police state of a RUNNING ORIGINAL gta_sa.exe (v1.0 US) from outside, in the terms of the run build's `[gr] NAME B` snapshot (GameplayRegress.cpp), so the same
checks run on both exes. ReadProcessMemory only (no injection, nothing written).

usage: python -X utf8 -I orig_gr_poll.py <out-file> [interval-s=1.0] [process=gta_sa.exe] [max-s=900] [tp-after-dismount-s=0]
With tp-after-dismount-s > 0 the player is teleported to the Idlewood street by the Pizza Stack (2100,-1790,13.5; the open street of the routes' cmd:tp) that many seconds after he has left a vehicle for the first time (the scenario mounts and dismounts the tutorial BMX first, which ends the opening mission); the original has no injector.
One line per sample: `[gr-o] qpc=<ms> pl=1 x= y= z= hp= arm= pst= wl= chaos= invh= peds60= cops= copsDead= copsDeadAlive= copsCar= copsFoot= nearCop= law100= lawDrv= nearLaw= pickups=`
qpc is the QueryPerformanceCounter clock in ms = the clock of inputdrv.exe's `[mark]` lines (Python's perf_counter on Windows is the same counter).
Addresses (v1.0 US): player ped pointer 0xB7CD98, ped pool 0xB74490 (stride 0x7A4), vehicle pool 0xB74494 (stride 0xA18), pickups 0x9788C0 (stride 0x20), CPed: health +0x540, armour +0x548, wanted ptr +0x480
(CPlayerPedData), pedstate +0x530, pedtype +0x594, flags dword +0x46C (bit 8 = bInVehicle); CVehicle: model +0x22, driver +0x460; CWanted: chaos +0, wanted level +0x2C.
"""
import ctypes, ctypes.wintypes as wt, struct, sys, time, subprocess, os

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.OpenProcess.restype = wt.HANDLE
k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
k32.WriteProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
PED_DEAD, PED_DIE = 55, 54          # ePedState values of PEDSTATE_DIE / PEDSTATE_DEAD (source/game_sa/Enums/ePedState.h)
LAW_MODELS = (596, 597, 598, 599, 523, 427, 528, 601, 490)   # police cars LS/SF/LV/ranger, cop bike, enforcer, FBI truck, SWAT van, FBI rancher


def find_pid(name):
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq ' + name, '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
    for ln in out.splitlines():
        p = [x.strip('"') for x in ln.split('","')]
        if len(p) > 1 and p[0].lower() == name.lower():
            return int(p[1])
    return None


# GR_ORIG_TP=x,y,z overrides the teleport target (default: the Idlewood street; the interior leg uses the LSPD entrance marker)
TPXYZ = tuple(float(v) for v in os.environ.get('GR_ORIG_TP', '2100,-1790,13.5').split(','))


def main():
    out = open(sys.argv[1], 'w', buffering=1)
    interval = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
    name = sys.argv[3] if len(sys.argv) > 3 else 'gta_sa.exe'
    tmax = float(sys.argv[4]) if len(sys.argv) > 4 else 900
    tp_after = float(sys.argv[5]) if len(sys.argv) > 5 else 0
    seen_veh, off_since, tp_done, dbg_done = False, None, False, False
    h = None
    t0 = time.time()

    def rd(a, n):
        b = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t(0)
        ok = k32.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(got))
        return b.raw[:got.value] if ok and got.value == n else None

    def u32(a):
        b = rd(a, 4)
        return struct.unpack('<I', b)[0] if b else None

    def f32(a):
        b = rd(a, 4)
        return struct.unpack('<f', b)[0] if b else None

    def pos(p):
        m = u32(p + 0x14)
        b = rd(m + 0x30 if m else p + 4, 12)
        return struct.unpack('<fff', b) if b else None

    while time.time() - t0 < tmax:
        if not h:
            pid = find_pid(name)
            if not pid:
                time.sleep(1)
                continue
            h = k32.OpenProcess(0x1F0FFF if tp_after else 0x0410, False, pid)
            out.write('# attached pid %d\n' % pid)
        time.sleep(interval)
        pl = u32(0xB7CD98)
        if not pl:
            if find_pid(name) is None:
                out.write('# process gone\n')
                break
            continue
        invh_now = ((rd(pl + 0x46D, 1) or bytes(1))[0] & 1) != 0
        if invh_now:
            seen_veh, off_since = True, None
        elif seen_veh and off_since is None:
            off_since = time.time()
        if tp_after and seen_veh and off_since and not tp_done and time.time() - off_since > tp_after:
            m = u32(pl + 0x14)
            if m:
                n = ctypes.c_size_t()
                k32.WriteProcessMemory(h, ctypes.c_void_p(m + 0x30), struct.pack('<fff', *TPXYZ), 12, ctypes.byref(n))
                tp_done = True
                out.write('# teleported to Idlewood at qpc=%.0f' % (time.perf_counter() * 1000) + chr(10))
        pp = pos(pl)
        hp, arm = f32(pl + 0x540), f32(pl + 0x548)
        if pp is None or hp is None:
            continue
        pd = u32(pl + 0x480)
        w = u32(pd) if pd else None
        wl = u32(w + 0x2C) if w else None
        chaos = u32(w) if w else None
        pst = u32(pl + 0x530)
        invh = ((rd(pl + 0x46D, 1) or b'\0')[0] & 1)
        pool = u32(0xB74490)
        curarea = u32(0xB72914)
        cops40 = 0
        copsnopos = 0
        copdz = ''
        peds = cops = dead = deadAlive = copsCar = copsFoot = live = nopos = 0
        nearCop = 99999
        if pool:
            objs, bm, size = u32(pool), u32(pool + 4), u32(pool + 8)
            bmap = rd(bm, size) or bytes([0x80]) * size
            if not dbg_done:
                dbg_done = True
                out.write('# ped pool %#x objs %#x map %#x size %d live %d' % (pool, objs or 0, bm or 0, size or 0, sum(1 for x in bmap if not x & 0x80)) + chr(10))
            for i in range(size):
                if bmap[i] & 0x80:
                    continue
                p = objs + i * 0x7A4
                if p == pl:
                    continue
                live += 1
                pe = pos(p)
                if not pe:
                    nopos += 1
                    if u32(p + 0x594) == 6:      # a cop whose position cannot be read: still counted (REGRESS-FLAKES: the interior leg)
                        copsnopos += 1
                    continue
                d = sum((a - b) ** 2 for a, b in zip(pe, pp)) ** .5
                if d > 60:
                    continue
                peds += 1
                if u32(p + 0x594) != 6:      # PED_TYPE_COP
                    continue
                cops += 1
                php, pstate = f32(p + 0x540) or 0, u32(p + 0x530)
                if php <= 0:
                    dead += 1
                    if pstate not in (PED_DIE, PED_DEAD):
                        deadAlive += 1
                    continue
                nearCop = min(nearCop, int(d))
                if d <= 40 and (rd(p + 0x2F, 1) or bytes([255]))[0] == curarea:
                    cops40 += 1
                    if cops40 <= 8:
                        copdz += '%d,' % int((pe[2] - pp[2]) * 10)
                if (rd(p + 0x46D, 1) or b'\0')[0] & 1:
                    copsCar += 1
                else:
                    copsFoot += 1
        law = lawDrv = 0
        nearLaw = 99999
        vp = u32(0xB74494)
        if vp:
            vobjs, vbm, vsize = u32(vp), u32(vp + 4), u32(vp + 8)
            vmap = rd(vbm, vsize) or bytes([0x80]) * vsize
            for i in range(vsize):
                if vmap[i] & 0x80:
                    continue
                v = vobjs + i * 0xA18
                mdl = struct.unpack('<H', rd(v + 0x22, 2) or b'\0\0')[0]
                if mdl not in LAW_MODELS:
                    continue
                pv = pos(v)
                if not pv:
                    continue
                d = sum((a - b) ** 2 for a, b in zip(pv, pp)) ** .5
                if d > 100:
                    continue
                law += 1
                lawDrv += 1 if u32(v + 0x460) else 0
                nearLaw = min(nearLaw, int(d))
        pk = 0
        pkb = rd(0x9788C0, 620 * 0x20)
        if pkb:
            pk = sum(1 for i in range(620) if pkb[i * 0x20 + 0x1C])
        out.write('[gr-o] live=%d nopos=%d qpc=%.0f pl=1 x=%d y=%d z=%d hp=%d arm=%d pst=%s wl=%s chaos=%s invh=%d peds60=%d cops=%d copsDead=%d copsDeadAlive=%d copsCar=%d copsFoot=%d nearCop=%d law100=%d lawDrv=%d nearLaw=%d pickups=%d area=%s cops40=%d copdz=%s copsnopos=%d\n' % (
            live, nopos, time.perf_counter() * 1000, pp[0], pp[1], pp[2], hp, arm, pst, wl, chaos, invh, peds, cops, dead, deadAlive, copsCar, copsFoot, nearCop, law, lawDrv, nearLaw, pk, curarea, cops40, copdz or '-', copsnopos))


if __name__ == '__main__':
    main()
