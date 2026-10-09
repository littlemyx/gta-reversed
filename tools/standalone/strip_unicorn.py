#!/usr/bin/env python3
"""strip_unicorn.py - runs the exe's RpBuildMesh tristrip generator 0x7591D0 (+ helpers) under Unicorn (pip install unicorn capstone not needed).
Library: run_cases(exe_bytes, cases, real_sort) -> [[(mat_index, [indices])...] per case]
  case = dict(tris=[(v0,v1,v2,mat)...], mats=[alpha byte per material], tryAll=0/1, pad=0/1)
  real_sort=False stubs the CRT qsort 0x8247E0 out (triangle order = input order, meshes = runs of equal material) to test the generator alone;
  real_sort=True runs the exe's qsort + comparator 0x759640 (tags matId/texIdx/rasIdx/pipeIdx as RpGeometryUnlock builds them: matId = material index,
  all other tags 0, untextured materials; opaque = colour alpha 0xFF).
Environment: engine instance at [0xC97B24] with the memfuncs (+0x134 malloc, +0x138 free, +0x13C realloc, +0x144 FreeListAlloc, +0x148 FreeListFree)
emulated by bump allocation; RwFreeListCreate 0x801980 / Destroy 0x801B80 stubbed; plugin offset [0xC9B8C0] = 0x100."""
import sys, struct

BASE, END = 0x400000, 0xcb1000
ENG, HOOKS, STACK, HEAP, SENT = 0x5000000, 0x4000000, 0x2000000, 0x8000000, 0x6000000
HEAP_SIZE = 0x8000000

def load(exe_path):
    d = open(exe_path, 'rb').read()
    secs = [(0x401000, 0x400, 0x456400), (0x858000, 0x456800, 0x4A1C00 - 0x456800), (0x8A4000, 0x4A1C00, 0x40000)]
    return d, secs

def make_uc(d, secs):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP
    img = bytearray(END - BASE)
    for va, raw, sz in secs:
        img[va - BASE:va - BASE + sz] = d[raw:raw + sz]
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(BASE, END - BASE); uc.mem_write(BASE, bytes(img))
    for a, n in ((ENG, 0x10000), (HOOKS, 0x1000), (STACK, 0x100000), (HEAP, HEAP_SIZE), (SENT, 0x1000)):
        uc.mem_map(a, n)
    uc.mem_write(SENT, b'\xf4' * 16)
    uc.mem_write(0xc97b24, struct.pack('<I', ENG)); uc.mem_write(0xc9b8c0, struct.pack('<I', 0x100))
    st = {'brk': HEAP, 'n_alloc': 0}
    def alloc(sz):
        sz = (sz + 15) & ~15
        p = st['brk']; st['brk'] += max(sz, 16)
        assert st['brk'] < HEAP + HEAP_SIZE
        return p
    def arg(uc, i):
        return struct.unpack('<I', uc.mem_read(uc.reg_read(UC_X86_REG_ESP) + 4 + 4 * i, 4))[0]
    def ret(uc, eax):
        esp = uc.reg_read(UC_X86_REG_ESP); r = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        uc.reg_write(UC_X86_REG_EAX, eax); uc.reg_write(UC_X86_REG_ESP, esp + 4); uc.reg_write(UC_X86_REG_EIP, r)
    fl = {}
    def h_malloc(uc, *a): ret(uc, alloc(arg(uc, 0)))
    def h_free(uc, *a): ret(uc, 0)
    def h_realloc(uc, *a):
        p = alloc(arg(uc, 1)); old = arg(uc, 0)
        if old: uc.mem_write(p, bytes(uc.mem_read(old, min(arg(uc, 1), 0x100000))))
        ret(uc, p)
    def h_flalloc(uc, *a):
        sz = fl[arg(uc, 0)]; ret(uc, alloc(sz))
    def h_flfree(uc, *a): ret(uc, 0)
    def h_flcreate(uc, *a):
        h = len(fl) + 1; fl[h] = arg(uc, 0); ret(uc, h)
    def h_fldestroy(uc, *a): ret(uc, 1)
    table = {0x134: h_malloc, 0x138: h_free, 0x13c: h_realloc, 0x144: h_flalloc, 0x148: h_flfree}
    disp = {}
    for i, (off, fn) in enumerate(table.items()):
        va = HOOKS + 0x10 * i; uc.mem_write(ENG + off, struct.pack('<I', va)); disp[va] = fn
    disp[0x801980] = h_flcreate; disp[0x801b80] = h_fldestroy
    def h_code(uc, address, size, user):
        f = disp.get(address)
        if f: f(uc)
    uc.hook_add(UC_HOOK_CODE, h_code, begin=HOOKS, end=HOOKS + 0x100)
    for va in (0x801980, 0x801b80):
        uc.hook_add(UC_HOOK_CODE, h_code, begin=va, end=va)
    # qsort stub hook is optional: installed per run
    return uc, st, alloc, ret

def run_cases(d, secs, cases, real_sort):
    from unicorn import UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP
    out = []
    for c in cases:
        uc, st, alloc, ret = make_uc(d, secs)
        if not real_sort:
            uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: ret(uc, 0), begin=0x8247e0, end=0x8247e0)
        mats = []
        for alpha in c['mats']:
            p = alloc(32); uc.mem_write(p, b'\0' * 32); uc.mem_write(p + 7, bytes([alpha])); mats.append(p)
        n = len(c['tris'])
        arr = alloc(n * 0x14 + 16)
        for i, (a, b, cc, m) in enumerate(c['tris']):
            uc.mem_write(arr + i * 0x14, struct.pack('<HHHHI4H', a, b, cc, 0, mats[m], m, 0, 0, 0)[:0x14])
        bm = alloc(16); uc.mem_write(bm, struct.pack('<III', n, n, arr))
        esp = STACK + 0x80000 - 16
        uc.mem_write(esp, struct.pack('<IIII', SENT, bm, c['tryAll'], c['pad']))
        uc.reg_write(UC_X86_REG_ESP, esp)
        uc.emu_start(0x7591d0, SENT, count=2000000000)
        hdr = uc.reg_read(UC_X86_REG_EAX)
        flags, nm, ser, total, first = struct.unpack('<IHHII', uc.mem_read(hdr, 16))
        meshes = []
        for i in range(nm):
            ip, ni, mp = struct.unpack('<III', uc.mem_read(hdr + 16 + i * 12, 12))
            meshes.append((mats.index(mp), list(struct.unpack('<%dH' % ni, uc.mem_read(ip, ni * 2)))))
        assert flags == 1 and sum(len(m[1]) for m in meshes) == total, (flags, total)
        out.append(meshes)
    return out

if __name__ == '__main__':
    d, secs = load(sys.argv[1] if len(sys.argv) > 1 else 'gta_sa_compact.exe')
    print(run_cases(d, secs, [dict(tris=[(0, 1, 2, 0), (2, 1, 3, 0)], mats=[255], tryAll=0, pad=1)], False))
