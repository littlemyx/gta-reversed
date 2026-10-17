#!/usr/bin/env python3
"""PT-AUDIO: sample the mixer state of a RUNNING ORIGINAL gta_sa.exe (v1.0 US, the repo's reference exe) from outside, in the same terms as the run build's NOTSA_AUDIO_STATS lines.

usage: python -I tools/standalone/audio_stats_exe.py <out-file> [interval-s=1.0] [process=gta_sa.exe] [max-s=3600]
Reads (ReadProcessMemory, no injection) CAESoundManager at 0xB62CB0 (300 x CAESound of 0x74 B at +4, physically-playing list pointer at +0x87F4, loop start times at +0x8A58),
CAEAudioHardware at 0xB5F8B8 (faders at +0x410.. +0x424) and the timer (0xB7CB84, paused flag 0xB7CB49). One line per sample:
  t=<timer ms> act=.. phys=.. chUsed=.. want=.. stopReq=.. loop=.. virtLoop=.. unanc=.. oldest=.. >30s=.. | paused=.. man=.. | effM effF musM musF nonStrF strF
"""
import ctypes, ctypes.wintypes as wt, struct, sys, time, subprocess

SM, HW = 0xB62CB0, 0xB5F8B8
TIMER_MS, TIMER_PAUSED = 0xB7CB84, 0xB7CB49
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


def main():
    out = open(sys.argv[1], 'w', buffering=1)
    interval = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
    name = sys.argv[3] if len(sys.argv) > 3 else 'gta_sa.exe'
    tmax = float(sys.argv[4]) if len(sys.argv) > 4 else 3600
    t0 = time.time()
    h = None
    while time.time() - t0 < tmax:
        if not h:
            pid = find_pid(name)
            if not pid:
                time.sleep(1)
                continue
            h = k32.OpenProcess(0x0410, False, pid)  # QUERY_INFORMATION | VM_READ
            out.write('# attached pid %d\n' % pid)
        sm = rd(h, SM, 0x8CBC)
        hw = rd(h, HW, 0x430)
        tm = rd(h, TIMER_MS, 4)
        if sm is None or hw is None or tm is None:
            if find_pid(name) is None:
                out.write('# process gone\n')
                break
            time.sleep(interval)
            continue
        now = struct.unpack_from('<I', tm)[0]
        nalloc, = struct.unpack_from('<H', sm, 0)
        plist_ptr, = struct.unpack_from('<I', sm, 0x87F4)
        act = phys = stopReq = want = loop = virtLoop = unanc = 0
        for i in range(300):
            o = 4 + i * 0x74
            inuse, = struct.unpack_from('<H', sm, o + 0x58)
            if not inuse:
                continue
            flags, = struct.unpack_from('<H', sm, o + 0x56)
            physp, = struct.unpack_from('<h', sm, o + 0x5E)
            lv, = struct.unpack_from('<f', sm, o + 0x60)
            stop, = struct.unpack_from('<h', sm, o + 0x68)
            fd = sm[o + 0x54]
            ls, = struct.unpack_from('<h', sm, 0x8A58 + i * 2)
            act += 1
            phys += physp != 0
            stopReq += stop != 0
            unanc += (flags >> 1) & 1
            if ls != -1:
                loop += 1
                virtLoop += physp == 0
            if physp == 0 and stop == 0 and fd == 0 and lv > -60.0:
                want += 1
        chUsed = 0
        if plist_ptr and nalloc:
            pl = rd(h, plist_ptr, nalloc * 2)
            if pl:
                chUsed = sum(1 for v in struct.unpack('<%dh' % nalloc, pl) if v != -1)
        ce = rd(h, 0xB6C14C, 0x1978)
        cu = co = cl = cn_ = 0
        if ce:
            cn_, = struct.unpack_from('<i', ce, 0x204)
            for i in range(300):
                o = 0x208 + i * 0x14
                snd, = struct.unpack_from('<I', ce, o + 8)
                st = ce[o + 0x10]
                cu += snd != 0
                co += st == 1
                cl += st == 2
        man = sm[0x8CB5]
        paused = rd(h, TIMER_PAUSED, 1)
        f = struct.unpack_from('<6f', hw, 0x410)  # music master, effect master, music fader, effects fader, nonstream fader, stream fader
        out.write('t=%u coll=%d(one %d loop %d n=%d) act=%d phys=%d chUsed=%d/%d want=%d stopReq=%d loop=%d(virt %d) unanc=%d | timerPause=%d man=%d | effM=%.3f effF=%.3f musM=%.3f musF=%.3f nonStrF=%.3f strF=%.3f\n' % (
            now, cu, co, cl, cn_, act, phys, chUsed, nalloc, want, stopReq, loop, virtLoop, unanc, paused[0] if paused else -1, man, f[1], f[3], f[0], f[2], f[4], f[5]))
        time.sleep(interval)


if __name__ == '__main__':
    main()
