# usage: orig_dead.py <exe-name> <maxsecs> <outdir>
# ORIGINAL gta_sa.exe v1.0 compact: waits for player control, then (via remote threads calling the exe's own functions) spawns peds around the player,
# kills them with GenerateDamageEvent in several ways, requests d3d9px back-buffer screenshots and logs the corpses' pose fields (slope IK, state, matrix).
import ctypes, ctypes.wintypes as wt, struct, sys, time, subprocess, os, math
k = ctypes.windll.kernel32
k.OpenProcess.restype = ctypes.c_void_p; k.CreateToolhelp32Snapshot.restype = ctypes.c_void_p; k.VirtualAllocEx.restype = ctypes.c_void_p
k.VirtualAllocEx.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, wt.DWORD, wt.DWORD]
k.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
k.WriteProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
k.CreateRemoteThread.restype = ctypes.c_void_p
k.CreateRemoteThread.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_void_p, wt.DWORD, ctypes.c_void_p]
k.WaitForSingleObject.argtypes = [ctypes.c_void_p, wt.DWORD]
k.GetExitCodeThread.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
exe, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
os.makedirs(out, exist_ok=True)
def find_pid(name):
    o = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq ' + name, '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
    for l in o.splitlines():
        p = l.replace('"', '').split(',')
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
def wr(a, data):
    n = ctypes.c_size_t(); return k.WriteProcessMemory(h, ctypes.c_void_p(a), data, len(data), ctypes.byref(n))
cave = k.VirtualAllocEx(h, None, 0x2000, 0x3000, 0x40)
data = cave + 0x1000   # scratch: vectors at data+0..0x3ff, results at data+0x400.., flag at data+0x7f0, orig eip slot at data+0x7f4
class TE(ctypes.Structure):
    _fields_ = [('dwSize', wt.DWORD), ('cntUsage', wt.DWORD), ('th32ThreadID', wt.DWORD), ('th32OwnerProcessID', wt.DWORD), ('tpBasePri', ctypes.c_long), ('tpDeltaPri', ctypes.c_long), ('dwFlags', wt.DWORD)]
def main_tid():
    snap = k.CreateToolhelp32Snapshot(4, 0); te = TE(); te.dwSize = ctypes.sizeof(TE); best = None
    ok = k.Thread32First(snap, ctypes.byref(te))
    while ok:
        if te.th32OwnerProcessID == pid:
            th = k.OpenThread(0x1FFFFF, False, te.th32ThreadID); c, e_, kk, u = (wt.FILETIME() for _ in range(4))
            k.GetThreadTimes(th, ctypes.byref(c), ctypes.byref(e_), ctypes.byref(kk), ctypes.byref(u))
            t = (c.dwHighDateTime << 32) | c.dwLowDateTime
            if best is None or t < best[0]: best = (t, te.th32ThreadID)
            k.CloseHandle(th)
        ok = k.Thread32Next(snap, ctypes.byref(te))
    k.CloseHandle(snap); return best[1]
class CTX(ctypes.Structure):   # x86 CONTEXT, only the control part is used (ContextFlags = CONTROL 0x10001)
    _fields_ = [('ContextFlags', wt.DWORD), ('Dr', wt.DWORD * 6), ('FloatSave', ctypes.c_ubyte * 112), ('SegGs', wt.DWORD), ('SegFs', wt.DWORD), ('SegEs', wt.DWORD), ('SegDs', wt.DWORD),
                ('Edi', wt.DWORD), ('Esi', wt.DWORD), ('Ebx', wt.DWORD), ('Edx', wt.DWORD), ('Ecx', wt.DWORD), ('Eax', wt.DWORD), ('Ebp', wt.DWORD), ('Eip', wt.DWORD), ('SegCs', wt.DWORD),
                ('EFlags', wt.DWORD), ('Esp', wt.DWORD), ('SegSs', wt.DWORD), ('Ext', ctypes.c_ubyte * 512)]
MT = k.OpenThread(0x1FFFFF, False, main_tid())
k.SuspendThread.argtypes = [ctypes.c_void_p]; k.ResumeThread.argtypes = [ctypes.c_void_p]
k.Wow64GetThreadContext.argtypes = [ctypes.c_void_p, ctypes.c_void_p]; k.Wow64SetThreadContext.argtypes = [ctypes.c_void_p, ctypes.c_void_p]; k.Wow64SuspendThread.argtypes = [ctypes.c_void_p]
k.OpenThread.restype = ctypes.c_void_p
def run_calls(calls, tmo=30.0):
    """calls = [(addr, [args])...]: runs them cdecl, in order, ON THE MAIN THREAD (suspend, redirect EIP to a stub that saves flags/regs/FPU, calls, restores, jumps back)."""
    code = bytes.fromhex('9c60' + '81ec6c000000' + 'dd3424' + 'd92c24')
    for i, (fn, args) in enumerate(calls):
        for a in reversed(args): code += b'\x68' + struct.pack('<I', a & 0xFFFFFFFF)
        code += b'\xB8' + struct.pack('<I', fn) + b'\xFF\xD0'
        if args: code += b'\x83\xC4' + bytes([4 * len(args)])
        code += b'\xA3' + struct.pack('<I', data + 0x400 + 4 * i)
    code += bytes.fromhex('c705') + struct.pack('<I', data + 0x7f0) + struct.pack('<I', 1)
    code += bytes.fromhex('dd2424' + '81c46c000000' + '61' + '9d' + 'ff25') + struct.pack('<I', data + 0x7f4)
    wr(data + 0x7f0, struct.pack('<I', 0))
    wr(cave, code)
    k.Wow64SuspendThread(MT)
    ctx = CTX(); ctx.ContextFlags = 0x10001
    if not k.Wow64GetThreadContext(MT, ctypes.byref(ctx)): raise RuntimeError("GetThreadContext failed %d" % k.GetLastError())
    wr(data + 0x7f4, struct.pack('<I', ctx.Eip))
    ctx.Eip = cave
    if not k.Wow64SetThreadContext(MT, ctypes.byref(ctx)): raise RuntimeError("SetThreadContext failed %d" % k.GetLastError())
    k.ResumeThread(MT)
    t0 = time.time()
    while time.time() - t0 < tmo:
        if u32(data + 0x7f0) == 1: break
        time.sleep(0.02)
    else:
        raise RuntimeError('stub did not finish')
    return [u32(data + 0x400 + 4 * i) for i in range(len(calls))]
def call_cdecl(addr, args, tmo=30.0):
    return run_calls([(addr, args)], tmo)[0]
def pedpool():
    pool = u32(0xB74490); objs, bm, size = u32(pool), u32(pool + 4), u32(pool + 8)
    bmap = rd(bm, size)
    return [objs + i * 0x7A4 for i in range(size) if not (bmap[i] & 0x80)]
def pos(p):
    m = u32(p + 0x14); b = rd(m + 0x30, 12) if m else None
    return struct.unpack('<fff', b) if b else None
def shot(name):
    sf = os.environ.get('D3D9PX_SHOTFILE')
    open(sf, 'w').write(os.path.join(out, name + '.bmp').replace('\\', '/') + '\n'); time.sleep(2.5)
log = open(os.path.join(out, 'dead.txt'), 'w')
def L(s):
    print(s, flush=True); log.write(s + '\n'); log.flush()
def dump(tag, rng=20):
    pl = u32(0xB7CD98); pp = pos(pl)
    for p in pedpool():
        if p == pl: continue
        pe = pos(p)
        if not pe or math.dist(pe, pp) > rng: continue
        st = u32(p + 0x530)
        if st not in (54, 55): continue
        m = u32(p + 0x14); mat = struct.unpack('<12f', rd(m, 48))
        fw = mat[4:7]; up = mat[8:11]
        n = struct.unpack('<3f', rd(p + 0x578, 12)); sp, sl = f32(p + 0x518), f32(p + 0x520)
        L('[dead] %s ped %#x model %d state %d hp %.0f pos=(%.2f,%.2f,%.2f) fwd=(%.2f,%.2f,%.2f) up=(%.2f,%.2f,%.2f) n578=(%.2f,%.2f,%.2f) slopePitch=%.3f slopeRoll=%.3f' % (tag, p, struct.unpack('<H', rd(p + 0x22, 2))[0], st, f32(p + 0x540), *pe, *fw, *up, *n, sp, sl))
def ctrl(): return rd(0xBA67A4, 1)[0] == 0 and rd(0xB5F851, 1)[0] == 0 and u32(0xC8D4C0) == 9
t0 = time.time(); cs = None
while time.time() - t0 < secs:
    time.sleep(0.5)
    pl = u32(0xB7CD98)
    if not pl: continue
    if ctrl():
        cs = cs or time.time()
        if time.time() - cs > 5: break
    else: cs = None
if not cs: L('[orig_dead] never got control'); sys.exit(1)
L('[orig_dead] control after %.0fs' % (time.time() - t0))
models = [7, 9, 10, 12, 13, 20, 21, 22, 23, 40, 41, 64, 65, 85, 87, 90, 92]
def invehicle(): return bool((u32(u32(0xB7CD98) + 0x46C) or 0) & 0x100) if u32(0xB7CD98) else False
# wait for the player to stand still on foot (the skip route can leave CJ on the BMX)
t1 = time.time(); still = None
while time.time() - t1 < 60:
    pl = u32(0xB7CD98); pa = pos(pl); time.sleep(1.0); pb = pos(pl)
    if pa and pb and math.dist(pa, pb) < 0.05 and not (u32(pl + 0x46C) & 0x100): still = still or time.time()
    else: still = None
    if still and time.time() - still > 3: break
L('[orig_dead] player still on foot: %s' % bool(still))
def spawn(n, rad, off):
    pl = u32(0xB7CD98); pp = pos(pl); res = []
    for i in range(n):
        mdl = models[i % len(models)]
        ang = math.radians(off) + i * 2 * math.pi / n
        p = (pp[0] + math.cos(ang) * rad, pp[1] + math.sin(ang) * rad, pp[2])
        wr(data + 12 * i, struct.pack('<3f', *p))
        r = run_calls([(0x4087E0, [mdl, 2]), (0x40EA10, [0]), (0x612710, [4, mdl, data + 12 * i, 0])])
        res.append(r[2]); L('[spawn] model %d ped %#x at (%.1f,%.1f,%.1f)' % (mdl, r[2], *p))
    return res
def killall(peds, weapon, piece, direction):
    pl = u32(0xB7CD98)
    for p in peds:
        ok = call_cdecl(0x73A530, [p, pl, weapon, 500, piece, direction])
        L('[kill] ped %#x weap %d piece %d dir %d -> %d' % (p, weapon, piece, direction, ok & 0xFF))
def lift(peds, hgt):
    for p in peds:
        pe = pos(p); m = u32(p + 0x14)
        if pe and m: wr(m + 0x38, struct.pack('<f', pe[2] + hgt))
def dump(tag, peds):
    for p in peds:
        pe = pos(p)
        if not pe: L('[dead] %s ped %#x gone' % (tag, p)); continue
        st = u32(p + 0x530)
        m = u32(p + 0x14); mat = struct.unpack('<12f', rd(m, 48))
        fw = mat[4:7]; up = mat[8:11]
        n = struct.unpack('<3f', rd(p + 0x578, 12)); sp, sl = f32(p + 0x518), f32(p + 0x520)
        L('[dead] %s ped %#x model %d state %d hp %.0f pos=(%.2f,%.2f,%.2f) fwd=(%.2f,%.2f,%.2f) up=(%.2f,%.2f,%.2f) n578=(%.2f,%.2f,%.2f) slopePitch=%.3f slopeRoll=%.3f' % (tag, p, struct.unpack('<H', rd(p + 0x22, 2))[0], st, f32(p + 0x540), *pe, *fw, *up, *n, sp, sl))
time.sleep(1)
g = spawn(8, 4, 0); time.sleep(3)
killall(g, 30, 3, 0); time.sleep(1); shot('g1_1s'); dump('g1_1s', g); time.sleep(2); dump('g1_4s', g); time.sleep(4); shot('g1_8s'); dump('g1_8s', g)
g = spawn(8, 7, 20); time.sleep(3); killall(g, 0, 3, 0); time.sleep(5); dump('g2_fist', g); shot('g2_fist')
g = spawn(8, 9, 10); time.sleep(3); killall(g, 5, 3, 2); time.sleep(5); dump('g3_bat', g); shot('g3_bat')
g = spawn(8, 5, 35); time.sleep(3); killall(g, 25, 9, 1); time.sleep(5); dump('g4_shotgun_head', g); shot('g4_shotgun_head')
g = spawn(8, 6, 15); time.sleep(3); lift(g, 35); time.sleep(6); dump('g5_fall', g); shot('g5_fall')
g = spawn(8, 4, 5); time.sleep(3); killall(g, 49, 3, 0); time.sleep(6); dump('g6_car', g); shot('g6_car')
L('[orig_dead] done')
