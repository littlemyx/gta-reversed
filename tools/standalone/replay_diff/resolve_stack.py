"""Resolve image-relative '+hex' offsets of the port's R lines (NOTSA_LOCKSTEP_RSTACK) to function names (+file:line) via dbghelp and the exe's pdb.
usage: py -I resolve_stack.py <gta_reversed.exe (with its .pdb next to it)> <trace> [--frame N] [--depth D]   -> table "count | R-site | caller chain"
Prints one row per distinct (site, resolved chain) of the R lines of frame N (default: all R lines in the file)."""
import ctypes, sys, os, collections
from ctypes import wintypes as W
exe, trace = sys.argv[1], sys.argv[2]
frame = int(sys.argv[sys.argv.index('--frame') + 1]) if '--frame' in sys.argv else None
depth = int(sys.argv[sys.argv.index('--depth') + 1]) if '--depth' in sys.argv else 4
dh = ctypes.WinDLL('dbghelp.dll')
dh.SymInitialize.argtypes = [W.HANDLE, W.LPCSTR, W.BOOL]
dh.SymLoadModuleEx.restype = ctypes.c_uint64
dh.SymLoadModuleEx.argtypes = [W.HANDLE, W.HANDLE, W.LPCSTR, W.LPCSTR, ctypes.c_uint64, W.DWORD, ctypes.c_void_p, W.DWORD]
dh.SymSetOptions(0x2 | 0x4 | 0x10000000 | 0x40000)  # UNDNAME | DEFERRED_LOADS... | LOAD_LINES; keep simple
H = ctypes.c_void_p(0x1234)
dh.SymInitialize(H, ('"' + os.path.dirname(os.path.abspath(exe)) + '"').strip('"').encode(), False)
BASE = 0x10000000
mod = dh.SymLoadModuleEx(H, None, os.path.abspath(exe).encode(), None, BASE, os.path.getsize(exe) + 0x1000000, None, 0)
class SYM(ctypes.Structure):
    _fields_ = [('SizeOfStruct', W.ULONG), ('TypeIndex', W.ULONG), ('Reserved', ctypes.c_uint64 * 2), ('Index', W.ULONG), ('Size', W.ULONG), ('ModBase', ctypes.c_uint64), ('Flags', W.ULONG),
                ('Value', ctypes.c_uint64), ('Address', ctypes.c_uint64), ('Register', W.ULONG), ('Scope', W.ULONG), ('Tag', W.ULONG), ('NameLen', W.ULONG), ('MaxNameLen', W.ULONG), ('Name', ctypes.c_char * 512)]
class LINE(ctypes.Structure):
    _fields_ = [('SizeOfStruct', W.DWORD), ('Key', ctypes.c_void_p), ('LineNumber', W.DWORD), ('FileName', ctypes.c_char_p), ('Address', ctypes.c_uint64)]
cache = {}
def res(off):
    if off in cache: return cache[off]
    s = SYM(); s.SizeOfStruct = 88; s.MaxNameLen = 511; d = ctypes.c_uint64()
    n = '?'
    if dh.SymFromAddr(H, ctypes.c_uint64(BASE + off), ctypes.byref(d), ctypes.byref(s)):
        n = s.Name.decode(errors='replace')
        l = LINE(); l.SizeOfStruct = ctypes.sizeof(LINE); dd = W.DWORD()
        if dh.SymGetLineFromAddr64(H, ctypes.c_uint64(BASE + off), ctypes.byref(dd), ctypes.byref(l)):
            n += ' (%s:%d)' % (os.path.basename(l.FileName.decode(errors='replace')), l.LineNumber)
    cache[off] = n
    return n
cnt = collections.OrderedDict()
for ln in open(trace, errors='replace'):
    if not ln.startswith('R '): continue
    t = ln.split()
    if frame is not None and int(t[1]) != frame: continue
    offs = [int(x[1:], 16) for x in t[3:] if x.startswith('+')]
    key = (t[2], ' < '.join(res(o) for o in offs[:depth]))
    cnt[key] = cnt.get(key, 0) + 1
for (site, chain), c in sorted(cnt.items(), key=lambda kv: -kv[1]):
    print('%6d  %-18s %s' % (c, site, chain))
