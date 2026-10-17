#!/usr/bin/env python3
"""Frame-time statistics from a d3d9px.bin (see d3d9px/d3d9px.c). Frame time = interval between consecutive Present calls; t_ms = absolute QPC clock (ms).
usage: frame_stats.py <d3d9px.bin> --trace                 one line per second since the first Present: frames, mean ms, max ms (find phases)
       frame_stats.py <d3d9px.bin> name:t0:t1 ...          windows in seconds since the first Present
       frame_stats.py <d3d9px.bin> --scenes <inputdrv.log> [--csv out.csv] [--tag RUN]
                                                          one row per '[scene TAG] qpc0 qpc1 cpu_ms=N' line of the inputdrv output
Columns: n frames, fps, p50/p95/p99/max ms, frames >50 / >100 ms, hitches per minute (>50 ms), draws/frame, ms inside Present, CPU ms/frame (scenes only)."""
import re, struct, sys
def load(p):
    d = open(p, 'rb').read(); n = len(d) // 16
    return [struct.unpack_from('<dfI', d, i * 16) for i in range(n)]
def pct(v, q): return v[min(len(v) - 1, int(len(v) * q))]
def stats(recs, t0, t1, cpu=None):
    sel = [r for r in recs if t0 <= r[0] < t1]
    if len(sel) < 3: return None
    ft = [sel[i][0] - sel[i - 1][0] for i in range(1, len(sel))]; s = sorted(ft); dur = (sel[-1][0] - sel[0][0]) / 1000
    h50 = sum(1 for x in ft if x > 50); h100 = sum(1 for x in ft if x > 100)
    return dict(n=len(ft), fps=len(ft) / dur, p50=pct(s, .5), p95=pct(s, .95), p99=pct(s, .99), mx=s[-1], h50=h50, h50m=h50 * 60 / dur, h100=h100,
                draws=sum(r[2] for r in sel) / len(sel), pres=sum(r[1] for r in sel) / len(sel), cpuf=(cpu / len(ft)) if cpu is not None else None, dur=dur)
def fmt(name, s):
    if not s: return '%-26s no frames' % name
    return '%-26s n=%5d fps=%6.2f p50=%6.2f p95=%6.2f p99=%7.2f max=%7.2f  >50ms=%3d (%.1f/min) >100ms=%3d  draws/f=%4.0f present=%5.2f ms%s' % (
        name, s['n'], s['fps'], s['p50'], s['p95'], s['p99'], s['mx'], s['h50'], s['h50m'], s['h100'], s['draws'], s['pres'], '' if s['cpuf'] is None else '  cpu/frame=%.1f ms' % s['cpuf'])
def main():
    a = sys.argv[1:]
    if not a: print(__doc__); return
    recs = load(a[0]); args = a[1:]
    if not recs: print('empty'); return
    base = recs[0][0]; end = (recs[-1][0] - base) / 1000
    if args == ['--trace']:
        t = 0
        while t < end:
            sel = [r for r in recs if base + t * 1000 <= r[0] < base + (t + 1) * 1000]
            ft = [sel[i][0] - sel[i - 1][0] for i in range(1, len(sel))]
            print('%4d s: %4d frames  mean %6.2f ms  max %7.2f ms' % (t, len(sel), sum(ft) / len(ft) if ft else 0, max(ft) if ft else 0)); t += 1
        return
    if args and args[0] == '--scenes':
        tag = args[args.index('--tag') + 1] if '--tag' in args else ''
        csvp = args[args.index('--csv') + 1] if '--csv' in args else None
        rows = []
        for ln in open(args[1], errors='replace'):
            m = re.search(r'\[scene (\S+)\] ([\d.]+) ([\d.]+) cpu_ms=(-?\d+)', ln)
            if m:
                s = stats(recs, float(m.group(2)), float(m.group(3)), float(m.group(4)))
                print(fmt(m.group(1), s)); rows.append((m.group(1), s))
        if csvp:
            with open(csvp, 'a') as f:
                for nm, s in rows:
                    if s: f.write('%s,%s,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%.1f,%d,%.0f,%.2f,%s\n' % (tag, nm, s['n'], s['fps'], s['p50'], s['p95'], s['p99'], s['mx'], s['h50'], s['h50m'], s['h100'], s['draws'], s['pres'], '' if s['cpuf'] is None else '%.2f' % s['cpuf']))
        return
    if not args: args = ['%d-%d:%d:%d' % (t, t + 30, t, t + 30) for t in range(0, int(end), 30)]
    for w in args:
        nm, x, y = w.split(':'); print(fmt(nm, stats(recs, base + float(x) * 1000, base + float(y) * 1000)))
main()
