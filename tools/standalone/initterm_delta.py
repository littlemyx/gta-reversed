#!/usr/bin/env python3
"""(tools/standalone copy; imported by extract_exe_data.py, also usable as a CLI)
initterm_delta.py <gta_sa_compact.exe> <out.json>   (pip install unicorn)
Runs the exe's own MSVC static-initialiser table (_initterm(__xc_a,__xc_z)) in Unicorn against the .data/.rdata initial image
(.bss = zero) and writes every byte range of .data/.bss (VA >= 0x8A4000) whose final value differs from the initial image:
  {"ranges":[[va,"hex"],...], "atexit":[dtorVA..], "rand_calls":N}
Environment modelled: x87 CW 0x27F (PC=53, what the CRT sets), rand() = MSVC LCG seeded 1, atexit recorded, security-cookie init skipped
(CRT-only, 0x8E31BC). Run order = table order. No Windows API is reachable from the initialisers (verified: 0 unmapped fetches)."""
import sys, struct, json
BASE, END = 0x400000, 0xcb1000
XC_A, XC_Z = 0x8a4000, 0x8a5a10

def compute(d, secs):
    """d = exe bytes, secs = [(va, raw_offset, raw_size)] of every section with raw data. Returns {'ranges','atexit','rand_calls'}"""
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_FETCH_UNMAPPED, UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE_UNMAPPED
    from unicorn.x86_const import UC_X86_REG_GDTR, UC_X86_REG_SS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_CS, UC_X86_REG_FS, UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP
    init = bytearray(END - BASE)
    for va, raw, sz in secs: init[va - BASE:va - BASE + sz] = d[raw:raw + sz]
    uc = Uc(UC_ARCH_X86, UC_MODE_32); uc.mem_map(BASE, END - BASE); uc.mem_write(BASE, bytes(init))
    STACK, FS, SENT, GDT = 0x2000000, 0x3000000, 0x4000000, 0x6000000
    for a, n in ((STACK, 0x100000), (FS, 0x10000), (SENT, 0x10000), (GDT, 0x1000)): uc.mem_map(a, n)
    uc.mem_write(FS, struct.pack('<I', 0xffffffff)); uc.mem_write(SENT, b'\xf4' * 16)
    def desc(b, l, ac, fl): return struct.pack('<HHBBBB', l & 0xffff, b & 0xffff, (b >> 16) & 0xff, ac, ((fl & 15) << 4) | ((l >> 16) & 15), (b >> 24) & 0xff)
    uc.mem_write(GDT, desc(0, 0, 0, 0) + desc(0, 0xfffff, 0xfb, 0xc) + desc(0, 0xfffff, 0xf3, 0xc) + desc(FS, 0xfff, 0xf3, 4) + desc(0, 0xfffff, 0x9b, 0xc) + desc(0, 0xfffff, 0x93, 0xc))
    uc.reg_write(UC_X86_REG_GDTR, (0, GDT, 0x30, 0))
    for r, v in ((UC_X86_REG_SS, 40), (UC_X86_REG_DS, 40), (UC_X86_REG_ES, 40), (UC_X86_REG_CS, 32), (UC_X86_REG_FS, 24)): uc.reg_write(r, v)
    uc.mem_write(SENT + 0x100, b'\xd9\x2d' + struct.pack('<I', SENT + 0x200) + b'\xf4'); uc.mem_write(SENT + 0x200, struct.pack('<H', 0x27f))
    uc.reg_write(UC_X86_REG_ESP, STACK + 0x80000); uc.emu_start(SENT + 0x100, SENT + 0x106)   # fldcw 0x27F
    atexit, rnd, hold = [], [0], [1]
    def ret_cdecl(uc, eax):
        esp = uc.reg_read(UC_X86_REG_ESP); ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        uc.reg_write(UC_X86_REG_EAX, eax); uc.reg_write(UC_X86_REG_ESP, esp + 4); uc.reg_write(UC_X86_REG_EIP, ret)
    def h_atexit(uc, a, s, u):
        esp = uc.reg_read(UC_X86_REG_ESP); atexit.append(struct.unpack('<I', uc.mem_read(esp + 4, 4))[0]); ret_cdecl(uc, 0)
    def h_rand(uc, a, s, u):
        hold[0] = (hold[0] * 0x343fd + 0x269ec3) & 0xffffffff; rnd[0] += 1; ret_cdecl(uc, (hold[0] >> 16) & 0x7fff)
    uc.hook_add(UC_HOOK_CODE, h_atexit, begin=0x821d1e, end=0x821d1f)   # atexit
    uc.hook_add(UC_HOOK_CODE, h_rand, begin=0x821b1e, end=0x821b1f)     # rand
    bad = []
    uc.hook_add(UC_HOOK_MEM_FETCH_UNMAPPED | UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED, lambda uc, ac, a, s, v, u: bad.append((ac, a)) or False)
    for ptr in range(XC_A, XC_Z, 4):
        fn = struct.unpack('<I', uc.mem_read(ptr, 4))[0]
        if fn in (0, 0x8339ca): continue                                 # NULL pad, __security_init_cookie
        esp = STACK + 0x80000; uc.reg_write(UC_X86_REG_ESP, esp - 4); uc.mem_write(esp - 4, struct.pack('<I', SENT))
        uc.emu_start(fn, SENT, count=50000000)
        assert uc.reg_read(UC_X86_REG_EIP) == SENT and not bad, (hex(ptr), hex(fn), bad)
    fin = bytes(uc.mem_read(BASE, END - BASE)); out = []; i = 0x8a4000 - BASE
    while i < len(fin):
        if fin[i] != init[i]:
            j = i
            while j < len(fin) and (fin[j] != init[j] or any(fin[k] != init[k] for k in range(j, min(j + 16, len(fin))))): j += 1
            out.append([i + BASE, fin[i:j].hex()]); i = j
        else: i += 1
    return {'ranges': out, 'atexit': atexit, 'rand_calls': rnd[0]}


if __name__ == '__main__':
    import extract_exe_data as e
    d = open(sys.argv[1], 'rb').read()
    _, secs = e.parse_pe(d)
    r = compute(d, [(s['va'], s['raw_offset'], s['raw_size']) for s in secs if s['name'] != '.rsrc'])
    json.dump(r, open(sys.argv[2], 'w'))
    print(len(r['ranges']), 'ranges,', sum(len(h) // 2 for _, h in r['ranges']), 'bytes,', len(r['atexit']), 'atexit, rand', r['rand_calls'])
