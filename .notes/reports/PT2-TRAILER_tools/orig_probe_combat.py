# usage: orig_probe.py <exe-name> <seconds>   ReadProcessMemory poller for the ORIGINAL gta_sa.exe (v1.0 compact): player hp/armour/wanted, cops nearby, pickups
import ctypes, ctypes.wintypes as wt, struct, sys, time
k = ctypes.windll.kernel32
k.OpenProcess.restype = ctypes.c_void_p; k.ReadProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p]; k.WriteProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p]
exe, secs = sys.argv[1], float(sys.argv[2])
tp_after = float(sys.argv[3]) if len(sys.argv) > 3 else 0   # teleport the player to Grove Street this many seconds after the player gets control
import subprocess
def find_pid(name):
    out = subprocess.run(['tasklist','/FI','IMAGENAME eq '+name,'/FO','CSV','/NH'],capture_output=True,text=True).stdout
    for l in out.splitlines():
        p = l.replace('"','').split(',')
        if len(p) > 1 and p[0].lower() == name.lower(): return int(p[1])
pid = None
for _ in range(120):
    pid = find_pid(exe)
    if pid: break
    time.sleep(1)
h = k.OpenProcess(0x1F0FFF, False, pid)
ctrl_since = None; tp_done = False
def rd(a, n):
    b = ctypes.create_string_buffer(n); r = ctypes.c_size_t()
    if not k.ReadProcessMemory(h, ctypes.c_void_p(a), b, n, ctypes.byref(r)): return None
    return b.raw
def u32(a):
    b = rd(a, 4); return struct.unpack('<I', b)[0] if b else None
def f32(a):
    b = rd(a, 4); return struct.unpack('<f', b)[0] if b else None
def pos(p):
    m = u32(p + 0x14)
    if not m: return None
    b = rd(m + 0x30, 12)
    return struct.unpack('<fff', b) if b else None
t0 = time.time(); lasthp = None
while time.time() - t0 < secs:
    time.sleep(0.25)
    pl = u32(0xB7CD98)
    if not pl: continue
    hp, arm = f32(pl + 0x540), f32(pl + 0x548)
    if hp is None: continue
    ctrl = (rd(0xBA67A4,1)[0]==0 and rd(0xB5F851,1)[0]==0 and u32(0xC8D4C0)==9)
    if ctrl and ctrl_since is None: ctrl_since = time.time()
    if tp_after and ctrl_since and not tp_done and time.time() - ctrl_since > tp_after:
        m = u32(pl + 0x14)
        if m:
            buf = struct.pack('<fff', 2495.0, -1688.0, 13.5); n = ctypes.c_size_t()
            r = k.WriteProcessMemory(h, ctypes.c_void_p(m + 0x30), buf, 12, ctypes.byref(n)); tp_done = True
            print('[tp] t=%.1f teleported (ok=%s)' % (time.time() - t0, r), flush=True)
    if (hp, arm) != lasthp:
        print('[dmg] t=%.1f hp %s arm %s' % (time.time() - t0, lasthp, (hp, arm)), flush=True); lasthp = (hp, arm)
    if int((time.time() - t0) * 4) % 4: continue
    pd = u32(pl + 0x480); w = u32(pd) if pd else None
    wl = u32(w + 0x2C) if w else None; chaos = u32(w) if w else None; flags = rd(w + 0x1E, 1)[0] if w else None
    pp = pos(pl)
    pst = u32(pl + 0x530)
    pool = u32(0xB74490)
    objs, bm, size = u32(pool), u32(pool + 4), u32(pool + 8)
    bmap = rd(bm, size) or bytes([0x80]) * size; cops = []; n = 0
    for i in range(size):
        if bmap[i] & 0x80: continue
        p = objs + i * 0x7C4
        if p == pl: continue
        pe = pos(p)
        if not pe or not pp: continue
        d = sum((a - b) ** 2 for a, b in zip(pe, pp)) ** .5
        if d > 60: continue
        n += 1
        typ = u32(p + 0x598)
        if typ in (6, 7, 22, 23, 24, 25) or d < 15:   # cop types / very near
            cops.append((typ, round(d), round(f32(p + 0x540) or 0), u32(p + 0x530)))
    vp = u32(0xB74494)
    if vp:
        vobjs, vbm, vsize = u32(vp), u32(vp + 4), u32(vp + 8)
        vmap = rd(vbm, vsize) or bytes([0x80]) * vsize
        vl = []
        for i in range(vsize):
            if vmap[i] & 0x80: continue
            v = vobjs + i * 0xA18
            mdl = struct.unpack('<H', rd(v + 0x22, 2))[0]
            pv = pos(v)
            if not pv or not pp: continue
            dd = sum((a1 - b1) ** 2 for a1, b1 in zip(pv, pp)) ** .5
            if dd > 120: continue
            sp = struct.unpack('<fff', rd(v + 0x44, 12))
            spd = (sp[0]**2 + sp[1]**2 + sp[2]**2) ** .5
            if mdl in (596, 597, 598, 599, 523, 427, 490, 528, 601):
                vl.append((mdl, round(dd), round(spd * 100)))
        print('[veh] t=%.0f lawcars(model,d,speed*100)=%s' % (time.time() - t0, vl), flush=True)
    pk = 0
    for i in range(620):
        if (rd(0x9788C0 + i * 0x20 + 0x1C, 1) or b'\0')[0]: pk += 1
    print('[probe] t=%.0f pos=%s state=%s cut=%d hp=%.0f arm=%.0f wanted=%s chaos=%s flags=%s peds60=%d near/cops(type,d,hp,state)=%s pickups=%d' % (time.time() - t0, tuple(round(x) for x in pp) if pp else None, pst, rd(0xB5F851,1)[0], hp, arm, wl, chaos, flags, n, cops[:8], pk), flush=True)
