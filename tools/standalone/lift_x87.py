#!/usr/bin/env python3
"""x86/x87 -> C++ lifter for the small RenderWare numeric routines of gta_sa_compact.exe (no jump tables, no SSE).

Used for the camera sync (0x7EE5A0 + its helpers) and RwMatrixOptimize (0x7F17E0): the exe evaluates them on the x87 stack, and the project rule
(PORT_BRIEF "Fidelity rules") is to keep the asm's expression order and float spill points. Hand-transcribing ~700 x87 instructions is where errors
creep in, so the instructions are translated one by one into straight C++ over an emulated register file / stack; the result is then verified
bit-for-bit against the exe's own machine code by the oracle in tests/standalone/rw_math_stream_test.cpp.

Semantics: x87 registers are `double` (the project's rule: 53-bit mantissa; each `fstp/fst dword` rounds to float), integer registers are `uintptr_t`
(the shim is a 32-bit build), memory is addressed with real pointers, the callee shares the caller's emulated stack (so cdecl arguments are read
exactly where the asm reads them). Absolute operands: .rdata dwords/qwords become literals, other absolute addresses go through MEMABS(addr).

usage: python3 -I lift_x87.py <gta_sa_compact.exe> <name=VA:END[,name=VA:END...]> [callmap name=VA ...]  > out.inc   (capstone-enabled python)
"""
import re
import struct
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_32

exe = open(sys.argv[1], 'rb').read()
FUNCS = []
for spec in sys.argv[2].split(','):
    n, r = spec.split('=')
    a, b = r.split(':')
    FUNCS.append((n, int(a, 16), int(b, 16)))
NAME_BY_VA = {va: n for n, va, _ in FUNCS}

# callee behaviour that is not lifted: VA -> C++ expression. `esp` is the emulated stack pointer AT the call (before the return address is pushed), so
# the first argument is at [esp]. They return a Ret{eax, st0} (st0 = valid only when `pushes_st0`).
EXTERN = {
    0x7EDB90: ('rwx::InvSqrt(RF32(esp))', True),     # _rwInvSqrt
    0x7EDB30: ('rwx::Sqrt(RF32(esp))', True),        # _rwSqrt
    0x808F60: ('RwxBBoxCalc(esp)', False),           # RwBBoxCalculate-like (out, vertices, n)
    0x7EDD90: ('RwxTransformPoints(esp)', False),    # RwV3dTransformPoints dispatch
}

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = False


def raw(va):
    return va - 0x401000 + 0x400


def rdata_dword(va):
    return struct.unpack('<I', exe[va - 0x858000 + 0x456800:][:4])[0]


def rdata_qword(va):
    return struct.unpack('<Q', exe[va - 0x858000 + 0x456800:][:8])[0]


REG32 = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')
REG8L = {'al': 'eax', 'cl': 'ecx', 'dl': 'edx', 'bl': 'ebx'}
REG8H = {'ah': 'eax', 'ch': 'ecx', 'dh': 'edx', 'bh': 'ebx'}
REG16 = {'ax': 'eax', 'cx': 'ecx', 'dx': 'edx', 'bx': 'ebx'}


def mem_expr(op):
    """'dword ptr [esi + eax*4 + 0x10]' -> (size, c++ address expression, absolute VA or None)"""
    m = re.match(r'(?:(byte|word|dword|qword|tbyte) ptr )?\[(.*)\]$', op.strip())
    if not m:
        return None
    size = m.group(1)
    inner = m.group(2).replace(' ', '')
    terms = re.findall(r'([+-]?)([^+-]+)', inner)
    parts = []
    absva = None
    for sign, t in terms:
        if t in REG32:
            parts.append(('-' if sign == '-' else '+') + t)
        elif '*' in t:
            r, s = t.split('*')
            parts.append('+(%s*%s)' % (r, s))
        else:
            v = int(t, 16) if t.startswith('0x') else int(t)
            parts.append('%s0x%XU' % ('-' if sign == '-' else '+', v))
            if len(terms) == 1:
                absva = v
    expr = '(R)(0' + ''.join(parts) + ')'
    return size, expr, absva


def rd(size, expr, absva, fmt):
    """expression reading memory"""
    if absva is not None and 0x858000 <= absva < 0x8A4000:       # .rdata constant
        if size == 'qword':
            return 'rwx::FromBits64(0x%XULL)' % rdata_qword(absva) if fmt == 'f' else '0x%XULL' % rdata_qword(absva)
        v = rdata_dword(absva)
        return ('rwx::FromBits(0x%XU)' % v) if fmt == 'f' else ('0x%XU' % v)
    if absva is not None:
        return 'MEMABS(0x%X)' % absva if fmt != 'f' else 'rwx::FromBits(MEMABS(0x%X))' % absva
    if fmt == 'f':
        return {'dword': 'RF32(%s)', 'qword': 'RF64(%s)'}[size] % expr
    return {'dword': 'RU32(%s)', 'word': 'RU16(%s)', 'byte': 'RU8(%s)', None: 'RU32(%s)'}[size] % expr


def wr(size, expr, absva, val):
    if absva is not None:
        raise SystemExit('write to absolute address %x' % absva)
    return {'dword': 'WU32(%s, %s);', 'word': 'WU16(%s, %s);', 'byte': 'WU8(%s, %s);', None: 'WU32(%s, %s);'}[size] % (expr, val)


def getreg(r):
    if r in REG32:
        return r
    if r in REG8L:
        return '(%s & 0xFFU)' % REG8L[r]
    if r in REG8H:
        return '((%s >> 8) & 0xFFU)' % REG8H[r]
    if r in REG16:
        return '(%s & 0xFFFFU)' % REG16[r]
    raise SystemExit('reg ' + r)


def setreg(r, v):
    if r in REG32:
        return '%s = (R)(%s);' % (r, v)
    if r in REG8L:
        g = REG8L[r]
        return '%s = (%s & ~(R)0xFFU) | (R)((%s) & 0xFFU);' % (g, g, v)
    if r in REG8H:
        g = REG8H[r]
        return '%s = (%s & ~(R)0xFF00U) | (R)(((%s) & 0xFFU) << 8);' % (g, g, v)
    if r in REG16:
        g = REG16[r]
        return '%s = (%s & ~(R)0xFFFFU) | (R)((%s) & 0xFFFFU);' % (g, g, v)
    raise SystemExit('reg ' + r)


def width(r):
    return 32 if r in REG32 else (16 if r in REG16 else 8)


def val(op, size_hint=None):
    """integer operand value expression (reads)"""
    op = op.strip()
    if op in REG32 or op in REG8L or op in REG8H or op in REG16:
        return getreg(op)
    if re.match(r'^-?(0x[0-9a-f]+|\d+)$', op):
        v = int(op, 16) if 'x' in op else int(op)
        return '0x%XU' % (v & 0xFFFFFFFF)
    m = mem_expr(op)
    if m:
        return rd(m[0], m[1], m[2], 'i')
    raise SystemExit('operand ' + op)


def lift_function(name, lo, hi):
    code = exe[raw(lo):raw(hi)]
    insns = list(md.disasm(code, lo))
    if not insns or insns[-1].address + insns[-1].size != hi:
        # data / padding in the range: tolerate only trailing padding
        end = insns[-1].address + insns[-1].size if insns else lo
        assert all(b in (0x90, 0xCC) for b in exe[raw(end):raw(hi)]), 'undecodable bytes in %s at %x' % (name, end)
    targets = set()
    for i in insns:
        if i.mnemonic.startswith('j') and i.mnemonic != 'jmp' or i.mnemonic == 'jmp':
            t = int(i.op_str, 16)
            targets.add(t)
    out = []
    top = 12                      # x87 stack: fp[top] = ST(0)
    top_at = {}
    pushes_st0 = False

    def st(i):
        return 'fp[%d]' % (top + i)

    out.append('static Ret %s(R esp) {' % name)
    out.append('    R eax = 0, ecx = 0, edx = 0, ebx = 0, ebp = 0, esi = 0, edi = 0; double fp[24] = {}; unsigned fsw = 0; bool zf = false, sf = false, cf = false, of = false, pf = false; (void)zf; (void)sf; (void)cf; (void)of; (void)pf; (void)fsw;')
    dead = False
    for i in insns:
        a = i.address
        mn, ops = i.mnemonic, i.op_str
        if a in targets:
            if a in top_at and not dead:
                assert top_at[a] == top, '%s: x87 depth mismatch at %x' % (name, a)
            if dead:
                top = top_at[a]
            top_at.setdefault(a, top)
            out.append('L_%X:;' % a)
            dead = False
        elif dead:
            # unreachable instruction after ret/jmp (padding / dead code): skip
            continue
        c = '    // %x: %s %s' % (a, mn, ops)
        out.append(c)
        L = out.append
        opl = [o.strip() for o in re.split(r',(?![^\[]*\])', ops)] if ops else []
        if mn == 'nop':
            continue
        # ---------------- x87 ----------------
        if mn.startswith('f'):
            fmem = None
            if opl and '[' in opl[0]:
                fmem = mem_expr(opl[0])
            def fsrc(o):
                m = re.match(r'st\((\d)\)$', o)
                if m:
                    return st(int(m.group(1)))
                mm = mem_expr(o)
                return rd(mm[0], mm[1], mm[2], 'f')
            if mn == 'fld':
                src = fsrc(opl[0])
                top -= 1
                L('    %s = %s;' % (st(0), src.replace('fp[%d]' % (top + 0), 'fp[%d]' % top) if False else src))
                # careful: fld st(i) reads ST(i) BEFORE the push: fix index
                if opl[0].startswith('st('):
                    k = int(re.match(r'st\((\d)\)', opl[0]).group(1))
                    out[-1] = '    %s = fp[%d];' % (st(0), top + 1 + k)
            elif mn == 'fld1':
                top -= 1; L('    %s = 1.0;' % st(0))
            elif mn == 'fldz':
                top -= 1; L('    %s = 0.0;' % st(0))
            elif mn in ('fst', 'fstp'):
                o = opl[0]
                m = re.match(r'st\((\d)\)$', o)
                if m:
                    L('    %s = %s;' % (st(int(m.group(1))), st(0)))
                else:
                    mm = mem_expr(o)
                    if mm[0] == 'dword':
                        L('    ' + wr('dword', mm[1], mm[2], 'rwx::Bits((float)%s)' % st(0)))
                    elif mm[0] == 'qword':
                        L('    WF64(%s, %s);' % (mm[1], st(0)))
                    else:
                        raise SystemExit('fst size')
                if mn == 'fstp':
                    top += 1
            elif mn == 'fxch':
                i1 = int(re.match(r'st\((\d)\)', opl[0]).group(1)) if opl else 1
                L('    { double t = %s; %s = %s; %s = t; }' % (st(0), st(0), st(i1), st(i1)))
            elif mn == 'fchs':
                L('    %s = -%s;' % (st(0), st(0)))
            elif mn == 'fabs':
                L('    %s = std::fabs(%s);' % (st(0), st(0)))
            elif mn in ('fadd', 'fmul', 'fsub', 'fsubr', 'fdiv', 'fdivr'):
                sym = {'fadd': '+', 'fmul': '*', 'fsub': '-', 'fsubr': '-', 'fdiv': '/', 'fdivr': '/'}[mn]
                rev = mn in ('fsubr', 'fdivr')
                if len(opl) == 1:
                    src = fsrc(opl[0])
                    dst = st(0)
                    L('    %s = %s;' % (dst, ('(%s) %s %s' % (src, sym, dst)) if rev else ('%s %s (%s)' % (dst, sym, src))))
                else:
                    # two explicit register operands: dst, src
                    d = int(re.match(r'st\((\d)\)', opl[0]).group(1))
                    s = int(re.match(r'st\((\d)\)', opl[1]).group(1))
                    L('    %s = %s;' % (st(d), ('%s %s %s' % (st(s), sym, st(d))) if rev else ('%s %s %s' % (st(d), sym, st(s)))))
            elif mn in ('faddp', 'fmulp', 'fsubp', 'fsubrp', 'fdivp', 'fdivrp'):
                sym = {'faddp': '+', 'fmulp': '*', 'fsubp': '-', 'fsubrp': '-', 'fdivp': '/', 'fdivrp': '/'}[mn]
                rev = mn in ('fsubrp', 'fdivrp')
                d = int(re.match(r'st\((\d)\)', opl[0]).group(1)) if opl else 1
                L('    %s = %s;' % (st(d), ('%s %s %s' % (st(0), sym, st(d))) if rev else ('%s %s %s' % (st(d), sym, st(0)))))
                top += 1
            elif mn in ('fcom', 'fcomp', 'fcompp', 'fucom', 'fucomp', 'fucompp'):
                if mn.endswith('pp'):
                    b = st(1)
                elif opl:
                    b = fsrc(opl[0])
                else:
                    b = st(1)
                L('    fsw = (%s > %s) ? 0u : (%s < %s) ? 0x100u : (%s == %s) ? 0x4000u : 0x4500u;' % (st(0), b, st(0), b, st(0), b))
                if mn.endswith('pp'):
                    top += 2
                elif mn.endswith('p'):
                    top += 1
            elif mn == 'fnstsw':
                L('    eax = (eax & ~(R)0xFFFFU) | fsw;')
            elif mn in ('fild', 'fistp', 'fldcw', 'fnstcw', 'fprem', 'frndint', 'fsqrt'):
                raise SystemExit('unsupported x87 %s at %x' % (mn, a))
            else:
                raise SystemExit('unknown x87 %s at %x' % (mn, a))
            continue
        # ---------------- integer ----------------
        if mn == 'mov' or mn == 'movzx':
            dst, src = opl
            if '[' in dst:
                m = mem_expr(dst)
                sz = m[0]
                if sz is None:
                    sz = 'dword'
                L('    ' + wr(sz, m[1], m[2], val(src)))
            else:
                if '[' in src:
                    m = mem_expr(src)
                    L('    ' + setreg(dst, rd(m[0], m[1], m[2], 'i')))
                else:
                    L('    ' + setreg(dst, val(src)))
        elif mn == 'lea':
            m = mem_expr(opl[1])
            L('    ' + setreg(opl[0], m[1]))
        elif mn == 'push':
            L('    esp -= 4; WU32(esp, %s);' % val(opl[0]))
        elif mn == 'pop':
            L('    ' + setreg(opl[0], 'RU32(esp)') + ' esp += 4;')
        elif mn in ('add', 'sub', 'and', 'or', 'xor', 'cmp', 'test', 'adc', 'sbb'):
            d, s = opl
            if '[' in d:
                m = mem_expr(d)
                sz = m[0] or 'dword'
                cur = rd(sz, m[1], m[2], 'i')
                bits = {'dword': 32, 'word': 16, 'byte': 8}[sz]
                dexpr = cur
            else:
                bits = width(d)
                dexpr = getreg(d)
            L('    { uint32_t a_ = (uint32_t)(%s), b_ = (uint32_t)(%s); uint32_t r_;' % (dexpr, val(s)))
            mask = {32: '0xFFFFFFFFu', 16: '0xFFFFu', 8: '0xFFu'}[bits]
            sign = {32: '0x80000000u', 16: '0x8000u', 8: '0x80u'}[bits]
            if mn in ('add',):
                L('      r_ = (a_ + b_) & %s; cf = (uint64_t)(a_ & %s) + (b_ & %s) > %s; of = ((a_ ^ r_) & (b_ ^ r_) & %s) != 0;' % (mask, mask, mask, mask, sign))
            elif mn in ('sub', 'cmp'):
                L('      r_ = (a_ - b_) & %s; cf = (a_ & %s) < (b_ & %s); of = ((a_ ^ b_) & (a_ ^ r_) & %s) != 0;' % (mask, mask, mask, sign))
            else:
                op_ = {'and': '&', 'or': '|', 'xor': '^', 'test': '&'}[mn]
                L('      r_ = (a_ %s b_) & %s; cf = false; of = false;' % (op_, mask))
            L('      zf = r_ == 0; sf = (r_ & %s) != 0; pf = PARITY8(r_);' % sign)
            if mn not in ('cmp', 'test'):
                if '[' in d:
                    L('      ' + wr(sz, m[1], m[2], 'r_'))
                else:
                    L('      ' + setreg(d, 'r_'))
            L('    }')
        elif mn in ('inc', 'dec'):
            d = opl[0]
            if '[' in d:
                raise SystemExit('inc mem')
            bits = width(d)
            mask = {32: '0xFFFFFFFFu', 16: '0xFFFFu', 8: '0xFFu'}[bits]
            sign = {32: '0x80000000u', 16: '0x8000u', 8: '0x80u'}[bits]
            sg = '+' if mn == 'inc' else '-'
            L('    { uint32_t a_ = (uint32_t)(%s); uint32_t r_ = (a_ %s 1u) & %s; of = %s; zf = r_ == 0; sf = (r_ & %s) != 0; pf = PARITY8(r_); %s }' % (
                getreg(d), sg, mask, ('r_ == %s' % sign) if mn == 'inc' else ('a_ == %s' % sign), sign, setreg(d, 'r_')))
        elif mn in ('sar', 'shl', 'shr'):
            d, s = opl
            bits = width(d)
            if bits != 32:
                raise SystemExit('shift width')
            e = {'sar': '(R)(int32_t)((int32_t)(%s) >> (%s))', 'shl': '(R)((uint32_t)(%s) << (%s))', 'shr': '(R)((uint32_t)(%s) >> (%s))'}[mn] % (getreg(d), val(s))
            L('    { uint32_t r_ = (uint32_t)(%s); %s zf = r_ == 0; sf = (r_ & 0x80000000u) != 0; pf = PARITY8(r_); }' % (e, setreg(d, 'r_')))
        elif mn == 'neg':
            d = opl[0]
            L('    { uint32_t r_ = 0u - (uint32_t)(%s); %s zf = r_ == 0; sf = (r_ & 0x80000000u) != 0; }' % (getreg(d), setreg(d, 'r_')))
        elif mn == 'jmp':
            L('    goto L_%X;' % int(opl[0], 16))
            dead = True
        elif mn.startswith('j'):
            cond = {
                'je': 'zf', 'jne': '!zf', 'jl': 'sf != of', 'jge': 'sf == of', 'jle': 'zf || sf != of', 'jg': '!zf && sf == of',
                'jb': 'cf', 'jae': '!cf', 'jbe': 'cf || zf', 'ja': '!cf && !zf', 'jp': 'pf', 'jnp': '!pf', 'js': 'sf', 'jns': '!sf',
            }[mn]
            t = int(opl[0], 16)
            L('    if (%s) goto L_%X;' % (cond, t))
            top_at.setdefault(t, top)
            assert top_at[t] == top, 'x87 depth mismatch on branch to %x from %x: %d vs %d' % (t, a, top_at[t], top)
        elif mn == 'call':
            t = int(opl[0], 16) if re.match(r'^0x', opl[0]) else None
            if t is None:
                raise SystemExit('indirect call at %x' % a)
            if t in EXTERN:
                expr, st0 = EXTERN[t]
                if st0:
                    top -= 1
                    L('    %s = (double)(%s);' % (st(0), expr))
                    # the x87 result is what the callee returned in ST(0); eax/ecx/edx are clobbered by the callee: not modelled (the asm never reads them)
                else:
                    L('    eax = (R)(%s);' % expr)
            elif t in NAME_BY_VA:
                L('    { Ret r_ = %s(esp - 4); eax = r_.eax; %s}' % (NAME_BY_VA[t], ''))
                if ST0RET.get(NAME_BY_VA[t]):
                    top -= 1
                    out[-1] = out[-1][:-1] + '%s = r_.st0; }' % st(0)
            else:
                raise SystemExit('call to unknown %x at %x' % (t, a))
        elif mn == 'ret':
            if top == 12:
                L('    return Ret{eax, 0.0};')
            else:
                assert top == 11, '%s: x87 depth %d at ret %x' % (name, 12 - top, a)
                pushes_st0 = True
                L('    return Ret{eax, %s};' % st(0))
            dead = True
        else:
            raise SystemExit('unknown insn %s %s at %x' % (mn, ops, a))
    out.append('}')
    return out, pushes_st0


ST0RET = {}
body = {}
for n, lo, hi in FUNCS:
    body[n], ST0RET[n] = lift_function(n, lo, hi)
print('// generated by tools/standalone/lift_x87.py from gta_sa_compact.exe (functions: %s) - do not edit; verified bit-exact by the exe oracle in tests/standalone/rw_math_stream_test.cpp' % ', '.join('%s=%X' % (n, va) for n, va, _ in FUNCS))
for n, lo, hi in FUNCS:
    print('\n'.join(body[n]))
