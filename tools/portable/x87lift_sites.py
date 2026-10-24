#!/usr/bin/env python3
"""x87lift_sites.py - the table of lifted inline-asm sites (MAC-F4a) and the two things generated from it.

  x87lift_sites.py gen                     write source/game_sa/Core/x87_lifted/<id>.inc (the lifted body of every site, tools/portable/x87lift.py)
  x87lift_sites.py harness OUTFILE         write the test harness include: one function per site holding (a) the ORIGINAL asm block, verbatim from the source (-DLIFT_ASM, MSVC x86)
                                           or (b) the lifted .inc (portable), plus the hand written glue (inputs / outputs) below. tests/portable/f4a_lift_test.cpp includes it.
  x87lift_sites.py wrap                    (one-off) insert `#ifdef NOTSA_PORTABLE / #include .inc / #else ... #endif` around every asm block in the sources (idempotent)

A site is (id, file, line of `__asm {`, line of its closing `}`); the lifted range is the lines between them.
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import x87lift

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
INC_DIR = 'source/game_sa/Core/x87_lifted'

# id: (file, open, close, ptr, sym, pc, glue)
# glue (harness): C++ statements; `$ASM` marks the place of the asm / lifted block; R = Rng&, o = Out&.  Names in the glue are those of the real function.
S = {}
def site(id, file, op, cl, glue, ptr='', sym='', pc=24, subst=None):
    S[id] = dict(file=file, open=op, close=cl, ptr=ptr, sym=sym, pc=pc, glue=glue, subst=subst or {})

M = 'source/game_sa/Core/Matrix.cpp'
MATRIX_IN = '''alignas(16) float mat[18]; for (auto& m : mat) m = R.flt();'''
MATRIX_OUT15 = 'for (int i = 0; i < 15; ++i) o.f(mat[i]);'
site('Matrix_SetRotate', M, 246, 320, '''
    ''' + MATRIX_IN + '''
    const float a_x = R.ang(), a_y = R.ang(), a_z = R.ang();
    float* self_ = mat;
    $ASM
    ''' + MATRIX_OUT15)
for nm, op, cl in (('X', 332, 400), ('Y', 415, 481), ('Z', 496, 564)):
    site('Matrix_Rotate' + nm, M, op, cl, '''
    ''' + MATRIX_IN + '''
    float A[1] = { R.ang() }; float L[8] = {};
    float* self = mat;
    $ASM
    ''' + MATRIX_OUT15)
site('Matrix_Rotate', M, 577, 763, '''
    ''' + MATRIX_IN + '''
    float A[3] = { R.ang(), R.ang(), R.ang() }; float L[21] = {};
    float* self = mat;
    $ASM
    ''' + MATRIX_OUT15)

TABLES = '''
    alignas(16) static const unsigned char T866D9C[96] = {0x00,0x00,0x48,0x42,0x00,0x00,0x00,0x36,0x01,0x02,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x0A,0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x70,0xCB,0x59,0x00};
'''
# the real tables are copied from the source at harness-generation time (see TABLE_RE)
site('Matrix_ConvertToEulerAngles', M, 855, 1051, '''
    $TABLES
    static const uint32_t K866D90 = 0x36000000u;
    alignas(16) float mat[18]; for (auto& m : mat) m = R.flt(); if (R.u32() % 4) { RotMat(R, mat); }
    float ox = -123.f, oy = -123.f, oz = -123.f;
    const float* a_pX = &ox; const float* a_pY = &oy; const float* a_pZ = &oz;   // the exe writes all three unconditionally
    const uint32_t a_uiFlags = R.u32() & 0x1F;
    float* self_ = mat;
    $ASM
    o.f(ox); o.f(oy); o.f(oz);''', ptr='a_pX,a_pY,a_pZ')
site('Matrix_ConvertFromEulerAngles', M, 1065, 1260, '''
    $TABLES
    ''' + MATRIX_IN + '''
    const float a_x = R.ang(), a_y = R.ang(), a_z = R.ang();
    const uint32_t a_uiFlags = R.u32() & 0x1F;
    float* self_ = mat;
    $ASM
    ''' + MATRIX_OUT15)

Q = 'source/game_sa/Core/Quaternion.cpp'
site('Quaternion_Slerp', Q, 126, 183, '''
    float halftheta = R.ang(), sintheta_inv = R.flt(), t = R.unit();
    if (R.u32() % 3 == 0) halftheta = R.unit();
    float from_[4], to_[4], dst[4]; for (auto& v : from_) v = R.unit(); for (auto& v : to_) v = R.unit(); for (auto& v : dst) v = -77.f;
    static const float halfPi = 1.57079637f, pi = 3.14159274f, one = 1.0f;
    float* self = dst; const float* pFrom = from_; const float* pTo = to_;
    $ASM
    for (float v : dst) o.f(v);''')
site('Quaternion_CalcThetaFromQuats', Q, 283, 330, '''
    float dot = R.unit(); if (R.u32() % 8 == 0) dot = R.flt();
    if (R.u32() % 16 == 0) dot = 1.0f; if (R.u32() % 16 == 1) dot = -1.0f; if (dot > 1.0f && R.u32() % 2) dot = 1.0f;
    const float ad = ad_(dot);
    const int mode = ad < 1.0f ? 0 : dot == 1.0f ? 1 : dot == -1.0f ? 2 : dot != dot ? 4 : 3;
    static const unsigned char qnan[10] = { 0, 0, 0, 0, 0, 0, 0, 0xC0, 0xFF, 0xFF };
    static const float zero = 0.0f, one = 1.0f;
    float theta = -5.f, invSinTheta = -5.f; float* pTheta = &theta; float* pInv = &invSinTheta;
    $ASM
    o.f(theta); o.f(invSinTheta);''')

C = 'source/game_sa/Cover.cpp'
site('Cover_FindDirFromVector', C, 442, 458, '''
    static const uint32_t K85A778 = 0x4222F983u;
    double res = 0;
    const float a_x = R.flt(), a_y = R.flt(), a_z = R.flt();
    $ASM
    o.d(res);''')
site('Cover_FindVectorFromDir', C, 470, 501, '''
    float out[3]{};
    const uint32_t raw = R.u32() & 0xFF;
    static const uint32_t K859BBC = 0x3CC90FDBu;
    const uint32_t a_raw = raw;
    $ASM
    o.f(out[0]); o.f(out[1]); o.f(out[2]);''', ptr='out')
site('Placeable_GetRoll', 'source/game_sa/Entity/Placeable.cpp', 112, 146, '''
    static const uint32_t K858B50 = 0x00000000u; static const uint32_t K858C1C = 0xBF800000u;
    alignas(16) float mat[18]; for (auto& m : mat) m = R.flt(); if (R.u32() % 2) { RotMat(R, mat); }
    struct Fake { unsigned char pad[0x14]; float* m_matrix; } fake{}; fake.m_matrix = (R.u32() % 16 == 0) ? nullptr : mat;
    float res = -9.f; const Fake* self_ = &fake;
    $ASM
    o.f(res);''', subst={'mov ecx, dword ptr [ecx + 0x14]': 'x87.ecx = (uintptr_t)(self_->m_matrix);   // CPlaceable::m_matrix (a 64 bit pointer in the portable layout)'})
site('Entity_GetIsTouching', 'source/game_sa/Entity/Entity.cpp', 1300, 1326, '''
    const float cbx = R.flt(), cby = R.flt(), cbz = R.flt(), boundRadius = R.flt(), radius = R.flt();
    float cv[3] = { R.flt(), R.flt(), R.flt() }; if (R.u32() % 4 == 0) { cv[0] = cbx; cv[1] = cby; cv[2] = cbz; }
    const float* c = cv;
    uint16_t status = 0;
    $ASM
    o.u(status & 0x4100);''')
site('Maths_SinTableEntry', 'source/game_sa/Maths.cpp', 17, 22, '''
    int32_t i = int32_t(R.u32() % 256); static const float step = bitf(0x3CC90FDBu); float res;
    $ASM
    o.f(res);''', sym='i=dword,step=dword,res=dword', pc=53)
site('PathFind_Heading', 'source/game_sa/PathFind.cpp', 2162, 2169, '''
    const float xv = R.unit(), yv = R.unit(); static const float radToDeg = 57.2957763671875f; float res;
    $ASM
    o.f(res);''', sym='xv=dword,yv=dword,radToDeg=dword,res=dword')
G = 'source/game_sa/Scripts/Commands/Ported/Group13_15.cpp'
site('Group13_DivideAssign', G, 396, 408, '''
    const float one = 1.0f; float v3[3] = { R.flt(), R.flt(), R.flt() }; float d = R.flt(); if (R.u32() % 8 == 0) d = bitf(0x3F800000u + (R.u32() & 0xFFFF));
    float* const p = v3;
    $ASM
    o.f(v3[0]); o.f(v3[1]); o.f(v3[2]);''')
site('Group13_AttractorDir', G, 417, 426, '''
    float degrees = R.flt(); if (R.u32() % 2) degrees = R.ang() * 60.f; const float k = 0.01745329238474369f; float x, y;
    $ASM
    o.f(x); o.f(y);''')
site('Group13_AcosCrt', G, 571, 602, '''
    double x = (double)R.unit(); if (R.u32() % 16 == 0) x = R.dbl(); if (R.u32() % 32 == 0) x = 1.0; if (R.u32() % 32 == 1) x = -1.0;
    const double ad = ad_(x);
    const int mode = ad < 1.0 ? 0 : x == 1.0 ? 1 : x == -1.0 ? 2 : x != x ? 4 : 3;
    static const unsigned char qnan[10] = { 0, 0, 0, 0, 0, 0, 0, 0xC0, 0xFF, 0xFF };
    double out;
    $ASM
    o.d(out);''')
site('Group13_Fsqrt', G, 608, 612, '''
    double v = (double)R.flt(); if (R.u32() % 4 == 0) v = R.dbl();
    $ASM
    o.d(v);''')
site('Group24b_SinCos', 'source/game_sa/Scripts/Commands/Ported/Group24b.cpp', 91, 98, '''
    double a = (double)R.ang(); if (R.u32() % 4 == 0) a = R.dbl(); double s, c;
    $ASM
    o.d(s); o.d(c);''')


def read_lines(f):
    return open(os.path.join(REPO, f), errors='replace').read().split('\n')


def locate(id):
    """(open, close) 1-based lines of the `__asm {` line and its closing brace: found through the include marker once the source is wrapped, else the table numbers"""
    s = S[id]; L = read_lines(s['file'])
    mark = f'#include "x87_lifted/{id}.inc"'
    op = s['open']
    for i, l in enumerate(L):
        if l.strip() == mark: op = i + 3; break      # marker, #else, __asm {
    assert '__asm' in L[op - 1], (id, op, L[op - 1])
    depth = 0; j = op - 1
    while True:
        depth += L[j].count('{') - L[j].count('}')
        if depth <= 0: break
        j += 1
    return op, j + 1


def lift(id):
    s = S[id]
    lf = x87lift.Lifter([p for p in s['ptr'].split(',') if p], s['pc'], dict(x.split('=') for x in s['sym'].split(',') if x), s['subst'])
    L = read_lines(s['file'])
    op, cl = locate(id)
    for l in L[op:cl - 1]: lf.line(l)
    hdr = f'// GENERATED by tools/portable/x87lift_sites.py (x87lift.py) from {s["file"]} [{id}] - the exe\'s x87 code lifted over notsa::fp::lift (X87Lift.h). Do not edit by hand.\n'
    return hdr + lf.result(s['pc'])


def cmd_gen():
    d = os.path.join(REPO, INC_DIR); os.makedirs(d, exist_ok=True)
    for id in S:
        open(os.path.join(d, id + '.inc'), 'w').write(lift(id))
    print(f'{len(S)} .inc written to {INC_DIR}')


def cmd_wrap():
    byfile = {}
    for id, s in S.items(): byfile.setdefault(s['file'], []).append(id)
    for f, ids in byfile.items():
        p = os.path.join(REPO, f); L = open(p, errors='replace').read().split('\n')
        todo = []
        for id in ids:
            if any(l.strip() == f'#include "x87_lifted/{id}.inc"' for l in L): continue
            op = S[id]['open']
            depth = 0; j = op - 1
            while True:
                depth += L[j].count('{') - L[j].count('}')
                if depth <= 0: break
                j += 1
            todo.append((op, j + 1, id))
        for op, cl, id in sorted(todo, reverse=True):
            L[cl:cl] = ['#endif']
            L[op - 1:op - 1] = ['#ifdef NOTSA_PORTABLE', f'#include "x87_lifted/{id}.inc"', '#else']
        open(p, 'w').write('\n'.join(L))
        print('wrapped', f, len(todo))


def cmd_harness(out):
    srcs = {}
    # tables of ConvertTo/FromEuler: copy the real ones
    mtx = read_lines(S['Matrix_ConvertToEulerAngles']['file'])
    tabs = [l for l in mtx[840:870] if 'T866D' in l and 'alignas' in l]
    tables = '\n'.join(tabs)
    res = ['// GENERATED by tools/portable/x87lift_sites.py harness - do not edit', '']
    names = []
    for id, s in S.items():
        L = read_lines(s['file'])
        op_, cl_ = locate(id)
        asm = [l for l in L[op_ - 1:cl_]]
        glue = s['glue'].replace('$TABLES', tables)
        # sites compiled for Euler share the table definitions: from-Euler does not redefine T866D95 etc., it uses the same three tables
        asm_txt = '\n'.join(asm)
        inc = f'#include "{id}.inc"'
        pcset = f'SetPc({s["pc"]});'
        body = glue.replace('$ASM', f'#ifdef LIFT_ASM\n    {pcset}\n{asm_txt}\n    SetPc(53);\n#else\n    (void)0;\n#include "x87_lifted/{id}.inc"\n#endif')
        res.append(f'static void site_{id}(Rng& R, Out& o) {{{body}\n}}')
        names.append(id)
    res.append('struct SiteDef { const char* name; void (*fn)(Rng&, Out&); };')
    res.append('static const SiteDef kSites[] = {' + ', '.join(f'{{"{n}", site_{n}}}' for n in names) + '};')
    open(out, 'w').write('\n'.join(res) + '\n')
    print('harness', out, len(names), 'sites')


if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'gen': cmd_gen()
    elif c == 'wrap': cmd_wrap()
    elif c == 'harness': cmd_harness(sys.argv[2])
    else: raise SystemExit(__doc__)
