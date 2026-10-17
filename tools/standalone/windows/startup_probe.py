#!/usr/bin/env python3
"""Cold-start probe for the standalone exe on Windows (WIN-PERF).
Launches <exe> in <datadir>, polls every 250 ms: wall time since CreateProcess, process CPU (user+kernel), I/O read bytes, working set,
gstate (0xC8D4C0), CTimer::m_FrameCounter (0xB7CB4C), size of logs/log.log; records the first time each gstate value / first frame / log line shows up.
Stops N seconds after the frame counter first moves (default 8), or after --timeout; then kills the process (NO graceful quit).
usage: startup_probe.py <exe> <datadir> [--timeout 240] [--after 8] [--env K=V ...] [--csv out.csv]
Take the GPU lock yourself (see PORT_BRIEF): this starts a fullscreen game."""
import argparse, ctypes, ctypes.wintypes as W, os, subprocess, sys, time, struct
k32 = ctypes.WinDLL('kernel32', use_last_error=True); ps = ctypes.WinDLL('psapi')
k32.OpenProcess.restype = W.HANDLE
class IOC(ctypes.Structure): _fields_ = [(n, ctypes.c_ulonglong) for n in ('ro', 'wo', 'oo', 'rb', 'wb', 'ob')]
class PMC(ctypes.Structure): _fields_ = [('cb', W.DWORD), ('pf', W.DWORD)] + [(n, ctypes.c_size_t) for n in ('peak', 'ws', 'pqpp', 'qpp', 'pqnp', 'qnp', 'pf2', 'ppf')]
def ft(a): return (a.dwHighDateTime << 32 | a.dwLowDateTime) / 1e4
def main():
    ap = argparse.ArgumentParser(); ap.add_argument('exe'); ap.add_argument('datadir'); ap.add_argument('--timeout', type=float, default=240)
    ap.add_argument('--after', type=float, default=8); ap.add_argument('--env', nargs='*', default=[]); ap.add_argument('--csv')
    a = ap.parse_args()
    env = dict(os.environ, NOTSA_STANDALONE_MUTE='1', NOTSA_STANDALONE_SKIP_VIDEOS='1')
    for kv in a.env: k, v = kv.split('=', 1); env[k] = v
    logp = os.path.join(a.datadir, 'logs', 'log.log')
    if os.path.exists(logp): os.remove(logp)
    t0 = time.perf_counter(); p = subprocess.Popen([os.path.join(a.datadir, a.exe)], cwd=a.datadir, env=env)
    h = k32.OpenProcess(0x1000 | 0x10 | 0x400, False, p.pid)   # QUERY_LIMITED | VM_READ | QUERY_INFORMATION
    rows = []; seen = {}; fc0 = None; tfc = None; last_loglen = 0
    def rd(addr):
        b = ctypes.create_string_buffer(4); n = ctypes.c_size_t()
        return struct.unpack('<I', b.raw)[0] if k32.ReadProcessMemory(h, ctypes.c_void_p(addr), b, 4, ctypes.byref(n)) else None
    while True:
        t = time.perf_counter() - t0
        if p.poll() is not None: print('# process exited rc=%s at %.1f s' % (p.returncode, t)); break
        c, e, kk, u = (W.FILETIME() for _ in range(4)); k32.GetProcessTimes(h, *(ctypes.byref(x) for x in (c, e, kk, u)))
        io = IOC(); k32.GetProcessIoCounters(h, ctypes.byref(io)); pm = PMC(); pm.cb = ctypes.sizeof(pm); ps.GetProcessMemoryInfo(h, ctypes.byref(pm), pm.cb)
        gs, fc = rd(0xC8D4C0), rd(0xB7CB4C); ll = os.path.getsize(logp) if os.path.exists(logp) else 0
        rows.append((t, ft(kk) + ft(u), io.rb / 1e6, pm.ws / 1e6, gs, fc, ll))
        if gs is not None and ('gs', gs) not in seen: seen[('gs', gs)] = t; print('%7.2f s  gstate -> %s' % (t, gs))
        if ll and not last_loglen: print('%7.2f s  log.log first bytes' % t)
        last_loglen = ll
        if fc is not None and fc0 is None and gs is not None: fc0 = fc
        if fc is not None and fc0 is not None and fc != fc0 and tfc is None: tfc = t; print('%7.2f s  FIRST FRAME (frame counter %s -> %s, gstate %s)' % (t, fc0, fc, gs))
        if tfc is not None and t - tfc >= a.after: break
        if t > a.timeout: print('# timeout'); break
        time.sleep(0.25)
    if p.poll() is None: subprocess.run(['taskkill', '/F', '/PID', str(p.pid)], capture_output=True)
    print('# first frame at %s s; cpu(ms)/io(MB)/ws(MB) at end: %s' % (('%.2f' % tfc) if tfc else 'never', rows[-1][1:4] if rows else None))
    if a.csv:
        with open(a.csv, 'w') as f:
            f.write('t,cpu_ms,io_read_mb,ws_mb,gstate,frame,loglen\n')
            for r in rows: f.write(','.join(str(x) for x in r) + '\n')
main()
