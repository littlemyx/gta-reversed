# usage: orig_probe_pti.py <exe-name> <seconds> <tp_after_ctrl_s>  -- ReadProcessMemory poller for the ORIGINAL gta_sa.exe (v1.0 compact)
# teleports the player to the LSPD entrance tp_after seconds after control, then logs every 0.5s: player pos/area/wanted, peds within 60m (pos, area, type, model, hp, state, task chain)
import ctypes, struct, sys, time, subprocess
k = ctypes.windll.kernel32
k.OpenProcess.restype = ctypes.c_void_p
k.ReadProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p]
k.WriteProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p]
exe, secs, tp_after = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
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
    b = rd(m + 0x30, 12) if m else rd(p + 4, 12)
    return struct.unpack('<fff', b) if b else None
def tasktype(t):
    vt = u32(t)
    if not vt: return -1
    fn = u32(vt + 4*4)
    if not fn: return -1
    b = rd(fn, 6)
    if b and b[0] == 0xB8 and b[5] == 0xC3: return struct.unpack('<I', b[1:5])[0]
    return -2
def contact(ped):
    c = u32(ped + 0x584)   # CPed::m_pContactEntity
    if not c: return 'none'
    a = rd(c + 0x2F, 1); m = rd(c + 0x22, 2)
    return '%x(model %d area %d)' % (c, struct.unpack('<H', m)[0] if m else -1, a[0] if a else -1)
def chain(ped):
    ints = u32(ped + 0x47C)
    if not ints: return ''
    tm = ints + 4
    out = []
    for i in range(5):
        t = u32(tm + 4*i)
        s = []; n = 0
        while t and n < 8:
            s.append(str(tasktype(t))); t = u32(t + 8) if tasktype(t) > 0 else 0; n += 1   # CTaskComplex::m_pSubTask at +8
        if s: out.append('p%d:%s' % (i, '>'.join(s)))
    return ' '.join(out)
t0 = time.time(); ctrl_since = None; tp_done = False; last = 0
while time.time() - t0 < secs:
    time.sleep(0.1)
    pl = u32(0xB7CD98)
    if not pl: continue
    ctrl = (rd(0xBA67A4,1)[0]==0 and rd(0xB5F851,1)[0]==0 and u32(0xC8D4C0)==9)
    if ctrl and ctrl_since is None: ctrl_since = time.time()
    if ctrl_since and not tp_done and time.time() - ctrl_since > tp_after:
        m = u32(pl + 0x14)
        if m:
            buf = struct.pack('<fff', 1554.95, -1675.0, 15.8); n = ctypes.c_size_t()
            r = k.WriteProcessMemory(h, ctypes.c_void_p(m + 0x30), buf, 12, ctypes.byref(n)); tp_done = True
            print('[tp] t=%.1f teleported to LSPD entrance (ok=%s)' % (time.time() - t0, r), flush=True)
    if time.time() - last < 0.5: continue
    last = time.time()
    hp, arm = f32(pl + 0x540), f32(pl + 0x548)
    if hp is None: continue
    pd = u32(pl + 0x480); w = u32(pd) if pd else None
    wl = u32(w + 0x2C) if w else None
    pp = pos(pl)
    if not pp: continue
    area = u32(0xB72914)
    print('[probe] t=%.1f player contact=%s pos=(%.1f,%.1f,%.1f) pedArea=%d currArea=%s hp=%.0f arm=%.0f wanted=%s state=%s' % (time.time()-t0, contact(pl), pp[0], pp[1], pp[2], (rd(pl+0x2F,1) or b'\xff')[0], area, hp, arm, wl, u32(pl+0x530)), flush=True)
    pool = u32(0xB74490)
    objs, bm, size = u32(pool), u32(pool + 4), u32(pool + 8)
    bmap = rd(bm, size) or bytes([0x80]) * size; n = 0
    for i in range(size):
        if bmap[i] & 0x80: continue
        p = objs + i * 0x7C4
        if p == pl: continue
        pe = pos(p)
        if not pe: continue
        d = sum((a - b) ** 2 for a, b in zip(pe, pp)) ** .5
        if d > 60: continue
        n += 1
        if n > 20: continue
        mdl = struct.unpack('<H', rd(p + 0x22, 2))[0]
        print('[ped] %x pos=(%.1f,%.1f,%.1f) d=%.1f area=%d type=%s model=%d hp=%.0f st=%s contact=%s tasks=%s' % (p, pe[0], pe[1], pe[2], d, (rd(p+0x2F,1) or b'\xff')[0], u32(p+0x598), mdl, f32(p+0x540) or 0, u32(p+0x530), contact(p), chain(p)), flush=True)
    print('[peds60] %d' % n, flush=True)
