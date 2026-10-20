#!/usr/bin/env python3
"""HOOK-ADDR: audit every RH_Scoped*Install address against the original exe - does it point at the first byte of a function?

usage: python -I tools/standalone/hook_addr_audit.py [--repo .] [--exe gta_sa_compact.exe] [--tsv out.tsv]
  needs python with `capstone`.  Uses oracle_coverage.inventory() for the site list.

Checks per site (exe = v1.0 compact exe, .text VA 0x401000..0x858000):
  RANGE      address outside .text
  DUP        the same address used by another port function (different fn name)
  FIRSTRET   first instruction is ret / int3 / nop / invalid (a bare ret or padding, not a function start)
  NOTBOUND   the byte(s) in front of the address are not a function boundary (ret / ret imm16 / int3 / nop padding / jmp, i.e. the previous
             function does not end exactly there) AND nothing references the address - the address is probably inside another function
  FALLTHRU   the previous instruction (decoded from the preceding 16 bytes) is not a terminator: execution of the previous code falls into addr
  INSIDE     another site's address lies inside this function's body (the linear walk from addr, extended by forward branch targets, passes it)
  REFS       informational: number of call/jmp rel32 sources and 4-byte data references to the address (raw byte scan)
Verdict OK = none of the flags above.  Everything else is printed for a manual look at the asm.
"""
import argparse, os, struct, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import oracle_coverage as oc

TEXT_LO, TEXT_HI = 0x401000, 0x858000
FILE_OFF = lambda va: va - 0x401000 + 0x400
DATA_LO, DATA_HI = 0x858000, 0xCB0000


def load_exe(path):
    return open(path, 'rb').read()


def byte_at(exe, va):
    return exe[FILE_OFF(va)]


def build_refs(exe):
    """addr -> [n_code_refs, n_data_refs] via raw scans (E8/E9 rel32 in .text, aligned dwords in .rdata/.data/.text)."""
    code = collections.Counter(); data = collections.Counter(); jmps = collections.Counter()
    base = FILE_OFF(TEXT_LO); end = FILE_OFF(TEXT_HI)
    t = exe[base:end]
    n = len(t)
    for op in (0xE8, 0xE9):
        i = t.find(bytes([op]))
        while i != -1:
            if i + 5 <= n:
                rel = struct.unpack_from('<i', t, i + 1)[0]
                tgt = TEXT_LO + i + 5 + rel
                if TEXT_LO <= tgt < TEXT_HI: (code if op == 0xE8 else jmps)[tgt] += 1
            i = t.find(bytes([op]), i + 1)
    # data refs: vtables / function pointer tables in the raw data of every non-.text section (section table walked, bss tails skipped)
    pe = struct.unpack_from('<I', exe, 0x3c)[0]; ns = struct.unpack_from('<H', exe, pe + 6)[0]; so = struct.unpack_from('<H', exe, pe + 20)[0]
    for k in range(ns):
        o = pe + 24 + so + k * 40
        name = exe[o:o + 8].rstrip(b'\x00'); rs, ro = struct.unpack_from('<II', exe, o + 16)
        if name == b'.text' or name == b'_TEXT_HA' or name == b'.rsrc': continue
        for off in range(ro, ro + rs - 3, 4):
            v = struct.unpack_from('<I', exe, off)[0]
            if TEXT_LO <= v < TEXT_HI: data[v] += 1
    # push imm32 / mov r32, imm32 in code (function pointers passed as arguments)
    for op in [0x68] + list(range(0xB8, 0xC0)):
        i = t.find(bytes([op]))
        while i != -1:
            if i + 5 <= n:
                v = struct.unpack_from('<I', t, i + 1)[0]
                if TEXT_LO <= v < TEXT_HI: data[v] += 1
            i = t.find(bytes([op]), i + 1)
    return code, data, jmps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', default=os.path.dirname(os.path.dirname(HERE)))
    ap.add_argument('--exe', default=None)
    ap.add_argument('--tsv', default=None)
    a = ap.parse_args()
    exe_path = a.exe
    if not exe_path:
        for c in (os.path.join(a.repo, 'gta_sa_compact.exe'), 'C:/Users/littl/github/gta-reversed/gta_sa_compact.exe'):
            if os.path.exists(c): exe_path = c; break
    exe = load_exe(exe_path)
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_GRP_JUMP, CS_GRP_RET, CS_GRP_CALL
    md = Cs(CS_ARCH_X86, CS_MODE_32); md.detail = True
    sites = oc.inventory(a.repo)
    code_refs, data_refs, jmp_refs = build_refs(exe)
    site_addrs = sorted(set(s['addr'] for s in sites))
    site_set = set(site_addrs)
    by_addr = collections.defaultdict(list)
    for s in sites: by_addr[s['addr']].append(s)

    def decode(va, n=64):
        o = FILE_OFF(va)
        return list(md.disasm(exe[o:o + n], va))

    def prev_is_boundary(va):
        """True when the bytes before va look like the end of the previous function (ret/ret imm/int3/nop/jmp) or an int3/nop pad."""
        b1 = byte_at(exe, va - 1)
        if b1 in (0xCC, 0x90, 0xC3): return True
        if byte_at(exe, va - 3) == 0xC2: return True              # ret imm16
        if byte_at(exe, va - 5) == 0xE9: return True              # jmp rel32 (tail jump)
        if byte_at(exe, va - 2) == 0xEB: return True              # jmp rel8
        if b1 == 0x00 and byte_at(exe, va - 2) == 0x00: return True  # zero padding
        d = struct.unpack_from('<I', exe, FILE_OFF(va - 4))[0]
        if TEXT_LO <= d < TEXT_HI: return True                       # a switch jump table (dword code pointers) directly in front
        return False

    def prev_insn_terminates(va):
        """Decode backwards by trying start offsets 1..15 before va; accept if some decode ends exactly at va and is ret/jmp/int3."""
        for back in range(1, 16):
            ins = list(md.disasm(exe[FILE_OFF(va - back):FILE_OFF(va)], va - back))
            if ins and ins[-1].address + ins[-1].size == va and len(ins) >= 1:
                last = ins[-1]
                if last.mnemonic in ('ret', 'retn', 'int3', 'nop', 'jmp', 'ud2', 'hlt'): return True
                if last.mnemonic == 'call':                      # noreturn call (e.g. __security_check) is possible: report as non-terminating
                    pass
        return False

    def extent(va, limit=0x4000):
        """end address of the function starting at va: linear walk, extended by forward branch targets inside the limit."""
        cur = va; furthest = va; pending = va
        seen = 0
        while cur < va + limit and seen < 20000:
            o = FILE_OFF(cur)
            ins = list(md.disasm(exe[o:o + 15], cur, 1))
            if not ins: break
            i = ins[0]; seen += 1
            nxt = cur + i.size
            for op in i.operands:
                if op.type == 2 and i.group(CS_GRP_JUMP) and not i.group(CS_GRP_RET):   # X86_OP_IMM = 2
                    t = op.imm
                    if va < t < va + limit and t > pending and i.mnemonic != 'call' and not (i.mnemonic == 'jmp' and (t in site_set or t in code_refs)): pending = t
            furthest = max(furthest, nxt)
            if i.group(CS_GRP_RET) or i.mnemonic in ('int3', 'ud2', 'hlt') or (i.mnemonic == 'jmp' and i.operands and i.operands[0].type != 2) or (i.mnemonic == 'jmp' and (i.operands[0].imm in site_set or i.operands[0].imm in code_refs or not (va < i.operands[0].imm < va + limit and i.operands[0].imm > nxt))):
                if nxt > pending: return nxt
            cur = nxt
        return furthest

    rows = []; stubs = []
    for s in sorted(sites, key=lambda x: (x['addr'], x['file'], x['line'])):
        ad = s['addr']; flags = []
        if not (TEXT_LO <= ad < TEXT_HI): flags.append('RANGE'); rows.append((s, flags, 0, 0, '')); continue
        fns = set((x['cls'], x['fn']) for x in by_addr[ad])
        # DUP: same address, a different port function (overloads/virtual re-registrations of the same cls::fn count as one)
        if len(fns) > 1: flags.append('DUP:' + '/'.join(sorted('%s::%s' % f if f[0] else f[1] for f in fns)))
        ins = decode(ad, 32)
        first = ins[0] if ins else None
        fa = ('%s %s' % (first.mnemonic, first.op_str)) if first else '(invalid)'
        c = code_refs.get(ad, 0); d = data_refs.get(ad, 0); j = jmp_refs.get(ad, 0)
        strong = c > 0 or d > 0
        boundary = prev_is_boundary(ad)
        is_ret = first is not None and first.mnemonic in ('ret', 'retn')
        if first is None or first.mnemonic in ('int3', 'nop', 'ud2', 'hlt') or first.bytes[:1] == b'\x00':
            flags.append('FIRSTRET')                                   # padding / invalid at the address
        elif is_ret:
            if boundary: stubs.append(s)                               # an empty function (ret / ret N) after a boundary: legitimate stub, counted
            else: flags.append('FIRSTRET')                             # a ret that ends the previous function (the SetClumpForAllAtomicsFlag case)
        if ad % 16 and not strong and not j and 'FIRSTRET' not in flags: flags.append('UNALIGNED-noref')   # not 16-aligned and nothing refers to it
        if not boundary and not strong and not j: flags.append('NOTBOUND')
        elif not boundary and not strong: flags.append('NOTBOUND-jmp-only')
        elif boundary and not strong and not prev_insn_terminates(ad) and not (TEXT_LO <= struct.unpack_from('<I', exe, FILE_OFF(ad - 4))[0] < TEXT_HI): flags.append('FALLTHRU')
        if not is_ret and 'FIRSTRET' not in flags and first is not None:
            end = extent(ad)
            inside = [x for x in site_addrs if ad < x < end]
            if inside: flags.append('INSIDE:' + ','.join('0x%X' % x for x in inside[:4]))
        rows.append((s, flags, c, d, fa))

    bad = [r for r in rows if r[1]]
    out = open(a.tsv, 'w', encoding='utf-8') if a.tsv else None
    print('sites=%d distinct_addrs=%d flagged_sites=%d' % (len(rows), len(site_addrs), len(bad)))
    cnt = collections.Counter(f.split(':')[0] for r in bad for f in r[1])
    print('flag counts:', dict(cnt), 'empty-stub sites (ret after a boundary, accepted):', len(stubs))
    for s, flags, c, d, fa in bad:
        line = '0x%06X\t%s::%s\t%s:%d\t%s\tcode_refs=%d data_refs=%d\t%s' % (s['addr'], s['cls'] or '', s['fn'], s['file'], s['line'], ' '.join(flags), c, d, fa)
        print(line)
        if out: out.write(line + '\n')
    if out: out.close()
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
