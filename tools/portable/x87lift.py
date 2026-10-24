#!/usr/bin/env python3
"""x87lift.py - lift an MSVC inline-asm block (the exe's x87 code, Intel syntax) into C++ over notsa::fp::lift::Cpu (source/game_sa/Core/X87Lift.h).

usage: x87lift.py FILE START END [--ptr sym,sym] [--pc 24] [--name NAME] [--out OUT.inc]
  FILE START END   the lines BETWEEN `__asm {` and its closing `}` (1-based, inclusive)
  --ptr            locals that hold POINTERS: `push dword ptr [sym]` pushes a handle (Cpu::Handle), `mov reg, sym` loads the native pointer
  --pc             x87 precision control of the Cpu (24 = game mode)
Output: a C++ statement list in its own block `{ Cpu c(pc); ... }`; it refers to the locals the asm refers to by name. Only the subset of x86 the exe's numeric routines
use is handled; anything else aborts (so a new construct cannot be silently mistranslated). Labels become C++ labels (only the referenced ones are emitted), `call` pushes
a dummy return address and the (single) `ret` of the callee jumps back after the call.
"""
import re, sys, argparse

REGS = {'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp'}
ARITH = {'fadd': 'Add', 'fsub': 'Sub', 'fsubr': 'SubR', 'fmul': 'Mul', 'fdiv': 'Div', 'fdivr': 'DivR'}


def num(s):
    s = s.strip()
    return int(s, 16) if s.lower().startswith(('0x', '-0x')) else int(s)


def split_ops(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '([': depth += 1
        if ch in ')]': depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip(): out.append(cur.strip())
    return out


class Lifter:
    def __init__(self, ptrs, pc, syms=None, subst=None):
        self.subst = {re.sub(r'\s+', ' ', k.strip().lower()): v for k, v in (subst or {}).items()}   # instruction text -> C++ (loads of pointer members: the portable layout differs from x86)
        self.syms = syms or {}
        self.ptrs = set(ptrs)
        self.pc = pc
        self.out = []
        self.labels_used = set()
        self.call_ret = None

    # ---- operands
    def addr(self, expr):
        """[expr] -> C++ uintptr_t expression"""
        expr = expr.replace(' ', '')
        terms = re.findall(r'([+-]?)([^+-]+)', expr)
        parts = []
        for sign, t in terms:
            if '*' in t:
                r, sc = t.split('*'); assert r in REGS, t
                p = f'(c.{r}*{sc})'
            elif t in REGS: p = f'c.{t}'
            elif re.fullmatch(r'(0x[0-9a-fA-F]+|\d+)', t): p = str(num(t)) + 'u' if sign != '-' else str(num(t))
            elif re.fullmatch(r'[A-Za-z_]\w*', t): p = f'AddrOf({t})'
            else: raise SystemExit(f'bad address term {t!r} in [{expr}]')
            parts.append(('-' if sign == '-' else '+') + p)
        s = ''.join(parts)
        if s.startswith('+'): s = s[1:]
        return f'(uintptr_t)({s})'

    def mem(self, op):
        if op.strip() in self.syms: return self.syms[op.strip()], f'AddrOf({op.strip()})'
        m = re.fullmatch(r'(?:(dword|qword|word|byte|tbyte)\s+ptr\s+)?\[(.+)\]', op.strip())
        if not m: return None
        return m.group(1), self.addr(m.group(2))

    def st(self, op):
        m = re.fullmatch(r'st(?:\((\d)\))?', op.strip())
        return int(m.group(1) or 0) if m else None

    def val(self, op):
        """integer rvalue (reg / imm / memory dword / symbol) as C++ expression"""
        op = op.strip()
        if op in REGS: return f'c.{op}'
        if re.fullmatch(r'-?(0x[0-9a-fA-F]+|\d+)', op): return f'(uintptr_t){num(op)}u' if num(op) >= 0 else f'(uintptr_t)(int32_t){num(op)}'
        m = self.mem(op)
        if m:
            sz, a = m
            if sz == 'byte': return f'c.R8({a})'
            assert sz in (None, 'dword'), op
            return f'c.R32({a})'
        if re.fullmatch(r'[A-Za-z_]\w*', op): return f'(uintptr_t)({op})'
        raise SystemExit(f'bad operand {op!r}')

    # ---- emit
    def emit(self, s): self.out.append('    ' + s)

    def line(self, raw):
        s = raw.split('//')[0].strip()
        if not s: return
        m = re.match(r'^([A-Za-z_]\w*):\s*(.*)$', s)
        if m and m.group(1) not in ('ret',):
            self.emit(f'@LABEL {m.group(1)}')
            s = m.group(2).strip()
            if not s: return
        mm = re.match(r'^(\w+)\s*(.*)$', s)
        mn, rest = mm.group(1).lower(), mm.group(2).strip()
        ops = split_ops(rest)
        key = re.sub(r'\s+', ' ', s.strip().lower())
        if key in self.subst: self.emit(self.subst[key]); return
        self.insn(mn, ops, s)

    def insn(self, mn, ops, src):
        e = self.emit
        # ---- x87
        if mn == 'fld':
            if self.st(ops[0]) is not None: e(f'c.f.LdSt({self.st(ops[0])});')
            else:
                sz, a = self.mem(ops[0]); e({'dword': f'c.f.LdF32(c.RF32({a}));', 'qword': f'c.f.LdF64(c.RF64({a}));', 'tbyte': f'c.f.Ld(notsa::fp::soft::FromBytes(c.At({a})));'}[sz])
        elif mn == 'fild':
            sz, a = self.mem(ops[0]); assert sz == 'dword', src
            e(f'c.f.LdI32((int32_t)c.R32({a}));')
        elif mn in ('fld1', 'fldz', 'fldpi', 'fldlg2', 'fldln2'):
            e(f'c.f.{mn[1:].capitalize()}();')
        elif mn in ('fst', 'fstp'):
            pop = mn == 'fstp'
            if self.st(ops[0]) is not None:
                i = self.st(ops[0]); e(f'c.f.{"StpSt" if pop else "StSt"}({i});')
            else:
                sz, a = self.mem(ops[0])
                if sz == 'dword': e(f'c.WF32({a}, c.f.St{"p" if pop else ""}F32());')
                elif sz == 'qword': e(f'c.WF64({a}, c.f.St{"p" if pop else ""}F64());')
                else: raise SystemExit(src)
        elif mn == 'fxch':
            e(f'c.f.Xch({self.st(ops[0]) if ops else 1});')
        elif mn == 'fchs': e('c.f.Chs();')
        elif mn == 'fsin': e('c.f.Sin();')
        elif mn == 'fcos': e('c.f.Cos();')
        elif mn == 'fsqrt': e('c.f.Sqrt();')
        elif mn == 'fpatan': e('c.f.Patan();')
        elif mn == 'fyl2x': e('c.f.Yl2x();')
        elif mn in ARITH or (mn.endswith('p') and mn[:-1] in ARITH):
            pop = mn not in ARITH
            op = ARITH[mn[:-1] if pop else mn]
            if len(ops) == 1:
                sz, a = self.mem(ops[0]); assert not pop, src
                e(f'c.f.Arith{"F32" if sz == "dword" else "F64"}(Op::{op}, c.RF{"32" if sz == "dword" else "64"}({a}));')
            else:
                d, s_ = self.st(ops[0]), self.st(ops[1]); assert d is not None and s_ is not None, src
                e(f'c.f.ArithSt(Op::{op}, {d}, {s_}{", true" if pop else ""});')
        elif mn in ('fcom', 'fcomp', 'fcompp'):
            pops = {'fcom': 0, 'fcomp': 1, 'fcompp': 2}[mn]
            if mn == 'fcompp': e('c.f.ComSt(1, 2);')
            elif self.st(ops[0]) is not None: e(f'c.f.ComSt({self.st(ops[0])}, {pops});')
            else:
                sz, a = self.mem(ops[0]); assert sz == 'dword', src
                e(f'c.f.ComF32(c.RF32({a}), {pops});')
        elif mn == 'fnstsw':
            if ops[0] == 'ax': e('c.FnstswAx();')
            else:
                sz, a = self.mem(ops[0]); assert sz == 'word', src
                e(f'c.W16({a}, c.f.Sw());')
        # ---- integer
        elif mn == 'mov':
            d, s_ = ops
            if d in REGS: e(f'c.{d} = {self.mem_or_ptr(s_)};')
            else:
                sz, a = self.mem(d); assert sz in (None, 'dword'), src
                e(f'c.W32({a}, (uint32_t){self.val(s_)});')
        elif mn == 'movzx':
            e(f'c.{ops[0]} = {self.val(ops[1])};')
        elif mn == 'lea':
            ms = re.fullmatch(r'\[\s*([A-Za-z_]\w*)\s*\]', ops[1])
            if ms and ms.group(1) in self.ptrs: e(f'c.{ops[0]} = c.Handle(reinterpret_cast<void*>(AddrOf({ms.group(1)})));')   # address of a local that the callee writes through
            else:
                sz, a = self.mem(ops[1]); e(f'c.{ops[0]} = {a};')
        elif mn == 'push':
            o = ops[0]
            ms = re.fullmatch(r'dword\s+ptr\s+\[\s*([A-Za-z_]\w*)\s*\]', o)
            if ms and ms.group(1) in self.ptrs: e(f'c.Push(c.Handle((void*){ms.group(1)}));')
            else: e(f'c.Push({self.val(o)});')
        elif mn == 'pop': e(f'c.{ops[0]} = c.Pop();')
        elif mn in ('add', 'sub', 'and', 'xor', 'shl', 'shr', 'or'):
            sym = {'add': '+', 'sub': '-', 'and': '&', 'xor': '^', 'shl': '<<', 'shr': '>>', 'or': '|'}[mn]
            r = ops[0]; assert r in REGS, src
            if mn == 'xor' and ops[1] == r: e(f'c.{r} = 0; c.SetZs(0);')
            elif r == 'esp': e(f'c.esp {sym}= {self.val(ops[1])};')
            else: e(f'c.{r} {sym}= {self.val(ops[1])}; c.SetZs(c.{r});')
        elif mn == 'cmp': e(f'c.Cmp({self.val(ops[0])}, {self.val(ops[1])});')
        elif mn == 'test':
            if ops[0] == 'ah': e(f'c.Test(c.Ah(), {self.val(ops[1])});')
            else: e(f'c.Test({self.val(ops[0])}, {self.val(ops[1])});')
        elif mn in ('jmp', 'jne', 'je', 'jp', 'jnp', 'jbe', 'ja', 'jb', 'jae'):
            cond = {'jmp': 'true', 'jne': '!c.zf', 'je': 'c.zf', 'jp': 'c.pf', 'jnp': '!c.pf', 'jbe': '(c.cf || c.zf)', 'ja': '!(c.cf || c.zf)', 'jb': 'c.cf', 'jae': '!c.cf'}[mn]
            self.labels_used.add(ops[0])
            e(f'if ({cond}) goto {ops[0]};' if mn != 'jmp' else f'goto {ops[0]};')
        elif mn == 'call':
            assert self.call_ret is None, 'two call sites'
            self.call_ret = 'L_RET_' + ops[0]
            self.labels_used.add(ops[0]); self.labels_used.add(self.call_ret)
            e(f'c.Push(0); goto {ops[0]};'); e(f'@LABEL {self.call_ret}')
        elif mn == 'ret':
            n = num(ops[0]) if ops else 0
            self.labels_used.add(self.call_ret)
            e(f'c.esp += {4 + n}; goto {self.call_ret};')
        else:
            raise SystemExit(f'unsupported instruction: {src}')

    def mem_or_ptr(self, s_):
        s_ = s_.strip()
        return self.val(s_)

    def result(self, pc):
        lines = []
        for l in self.out:
            m = re.match(r'\s*@LABEL (\S+)$', l)
            if m:
                if m.group(1) in self.labels_used: lines.append(f'    {m.group(1)}:;')
            else: lines.append(l)
        body = re.sub(r'\bc\.', 'x87.', '\n'.join(lines))     # the machine is `x87`: the asm may name a local `c`
        return f'{{\n    using namespace notsa::fp::lift;\n    Cpu x87({pc});\n' + body + '\n}\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file'); ap.add_argument('start', type=int); ap.add_argument('end', type=int)
    ap.add_argument('--ptr', default=''); ap.add_argument('--pc', type=int, default=24); ap.add_argument('--out')
    ap.add_argument('--name', default='')
    ap.add_argument('--sym', default='', help='bare-symbol memory operands: name=dword|qword,...')
    a = ap.parse_args()
    L = open(a.file, errors='replace').read().split('\n')[a.start - 1:a.end]
    lf = Lifter([p for p in a.ptr.split(',') if p], a.pc, dict(x.split('=') for x in a.sym.split(',') if x))
    for l in L: lf.line(l)
    hdr = f'// GENERATED by tools/portable/x87lift.py from {a.file}:{a.start}-{a.end} {a.name} - the exe\'s x87 code lifted over notsa::fp::lift (X87Lift.h). Do not edit by hand.\n'
    txt = hdr + lf.result(a.pc)
    if a.out: open(a.out, 'w').write(txt)
    else: sys.stdout.write(txt)


if __name__ == '__main__':
    main()
