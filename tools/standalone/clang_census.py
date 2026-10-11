#!/usr/bin/env python3
"""Stream C compiler census: run clang over every gta_reversed TU of a CMake build and collect the diagnostics.

Two modes (see .notes/STREAM_C_PLAN.md):
  msvc    clang-cl --target=i686-pc-windows-msvc, same defines / MSVC+SDK headers / conan headers as the MSVC build.
          Used to find what blocks a clang build of the Win32 x86 reference (stream C proper).
  native  clang++ -target arm64-apple-macos, C++23, headers of the Windows SDK replaced by tools/standalone/winstub.
          Syntax-only census used to COUNT and classify what blocks a native build (streams P/W/F/R/A).

The TU list and the defines come from <build-dir>/compile_commands.json (the MSVC build), so the census follows the
real build configuration.  Nothing is written outside --out.

  clang_census.py run     --mode msvc --build-dir build/StandaloneRelease --out DIR [-j 6] [--only REGEX] [--llvm /opt/homebrew/opt/llvm]
  clang_census.py report  --out DIR     (aggregate DIR/logs into DIR/errors.tsv + summary.json)

The run step is quiet and resumable (a TU whose log exists is skipped unless --force).
"""
import argparse, concurrent.futures, hashlib, json, os, re, shlex, subprocess, sys, time
from collections import Counter, defaultdict

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC = os.path.join(REPO, 'source') + os.sep
LLVM_DEFAULT = '/opt/homebrew/opt/llvm'


def load_tus(build_dir, only):
    cc = json.load(open(os.path.join(build_dir, 'compile_commands.json')))
    tus, seen = [], set()
    for e in cc:
        f = e['file']
        if not f.startswith(SRC) or '/libs/' in f:
            continue
        cmd = shlex.split(e['command'])
        fo = next((a for a in cmd if a.startswith('/Fo')), '')
        if 'gta_reversed.dir' not in fo:   # only the game target (tests re-compile the same files)
            continue
        if f in seen:
            continue
        seen.add(f)
        if only and not re.search(only, f):
            continue
        tus.append((f, cmd))
    return tus


def split_cmd(cmd):
    defs, incs, sysincs = [], [], []
    for a in cmd:
        if a.startswith('-D'):
            defs.append(a)
        elif a.startswith('/D'):
            defs.append('-D' + a[2:])
        elif a.startswith('-external:I'):
            p = a[len('-external:I'):]
            # the MSVC STL / SDK are replaced by clang's own selection; keep conan + project externals
            sysincs.append(p)
        elif a.startswith('-I'):
            incs.append(a[2:])
    return defs, incs, sysincs


def write_pch_hxx(out, mode):
    """The cmake_pch.hxx of the MSVC build only includes StdInc.cpp; the census uses an own PCH per mode."""
    p = os.path.join(out, 'pch.hxx')
    open(p, 'w').write('#ifdef __cplusplus\n#include "%s"\n#endif\n' % (SRC + 'StdInc.cpp'))
    return p


def msvc_flags(llvm, defs, incs, sysincs, extra):
    msvc = '/Users/Andrei.Mukhin/tools/msvc'
    # MSVC STL + SDK dirs exactly as the cl.exe command uses them
    return ([os.path.join(llvm, 'bin', 'clang-cl'), '--target=i686-pc-windows-msvc', '/nologo', '/TP', '/EHsc',
             '/std:c++20', '/clang:-std=c++23', '/Zc:__cplusplus', '/utf-8', '-fms-compatibility-version=19.44', '-w',
             '-fno-color-diagnostics', '-fno-caret-diagnostics', '-fdiagnostics-format=clang', '-ferror-limit=0', '/arch:IA32', '/fp:precise',
             '/DWIN32', '/D_WINDOWS', '/DNDEBUG', '/MT', '/bigobj']
            + defs + ['-I' + i for i in incs] + ['/imsvc' + i for i in sysincs] + extra)


STUB = os.path.join(REPO, 'tools', 'standalone', 'winstub')


def native_flags(llvm, defs, incs, sysincs, extra):
    # Apple clang is fine; brew LLVM is used when given (newer libc++, same front end)
    cxx = os.path.join(llvm, 'bin', 'clang++') if llvm and os.path.exists(os.path.join(llvm, 'bin', 'clang++')) else 'clang++'
    skip_sys = ('/tools/msvc/',)
    incs = [i for i in incs if not any(s in i for s in skip_sys)]
    sysincs = [i for i in sysincs if not any(s in i for s in skip_sys)]
    return ([cxx, '-target', 'arm64-apple-macos15', '-std=c++23', '-fsyntax-only', '-x', 'c++', '-Wno-everything', '-fno-color-diagnostics',
             '-fno-caret-diagnostics', '-ferror-limit=0', '-fms-extensions', '-fno-delayed-template-parsing',
             # the width bugs the census is about: warnings by default, made errors so that they are counted (-w hides everything else)
             '-Werror=pointer-to-int-cast', '-Werror=int-to-pointer-cast', '-Werror=pointer-integer-compare', '-Werror=int-conversion',
             '-isystem', STUB, '-include', os.path.join(STUB, 'winstub_prefix.h'), '-I/opt/homebrew/include']
            + [d for d in defs if not d.startswith('-D_MBCS')] + ['-I' + i for i in incs]
            + sum((['-isystem', i] for i in sysincs), []) + extra)


def run_one(args):
    mode, llvm, out, f, cmd, pch_flags, force, extra = args
    key = hashlib.md5(f.encode()).hexdigest()[:10] + '_' + os.path.basename(f)
    log = os.path.join(out, 'logs', key + '.log')
    if os.path.exists(log) and not force:
        return f, log, 0.0, None
    defs, incs, sysincs = split_cmd(cmd)
    if mode == 'msvc':
        argv = msvc_flags(llvm, defs, incs, sysincs, extra) + ['-fsyntax-only'] + pch_flags + ['--', f]
    else:
        argv = native_flags(llvm, defs, incs, sysincs, extra) + pch_flags + [f]
    t0 = time.time()
    try:
        p = subprocess.run(argv, capture_output=True, text=True, timeout=900, errors='replace')
        txt, rc = p.stdout + p.stderr, p.returncode
    except subprocess.TimeoutExpired:
        txt, rc = 'TIMEOUT\n', -1
    open(log, 'w').write('# %s rc=%d\n%s' % (f, rc, txt))
    return f, log, time.time() - t0, rc


def cmd_run(a):
    out = os.path.abspath(a.out)
    os.makedirs(os.path.join(out, 'logs'), exist_ok=True)
    tus = load_tus(a.build_dir, a.only)
    extra = shlex.split(a.extra or '')
    pch_flags = []
    if a.pch:
        # shared PCH over StdInc.h so that each TU only parses its own includes
        hxx = write_pch_hxx(out, a.mode)
        pch = os.path.join(out, 'pch.pch')
        defs, incs, sysincs = split_cmd(tus[0][1])
        if a.mode == 'msvc':
            base = msvc_flags(a.llvm, defs, incs, sysincs, extra)
            base = [x for x in base]
            cmd = base + ['/Yc' + hxx, '/Fp' + pch, '/Fo' + os.path.join(out, 'pch.obj'), '/c', '/FI' + hxx, '--', os.path.join(out, 'pch_dummy.cpp')]
            open(os.path.join(out, 'pch_dummy.cpp'), 'w').write('// pch\n')
            pch_flags = ['/Yu' + hxx, '/Fp' + pch, '/FI' + hxx]
        else:
            base = native_flags(a.llvm, defs, incs, sysincs, extra)
            base = [x for x in base if x != '-fsyntax-only']
            # errors inside the PCH are counted once (pch.log); -fallow-pch-with-compiler-errors lets the per-TU runs reuse a PCH that has errors
            cmd = base + ['-Xclang', '-fallow-pch-with-compiler-errors', '-x', 'c++-header', hxx, '-o', pch]
            pch_flags = ['-Xclang', '-fallow-pch-with-compiler-errors', '-include-pch', pch]
        if not os.path.exists(pch) or a.force or a.pch_only:
            t0 = time.time()
            p = subprocess.run(cmd, capture_output=True, text=True, errors='replace')
            open(os.path.join(out, 'pch.log'), 'w').write(p.stdout + p.stderr)
            print('PCH rc=%d %.0fs (log pch.log)' % (p.returncode, time.time() - t0), flush=True)
            if p.returncode != 0 and not os.path.exists(pch) or (p.returncode != 0 and a.mode == 'msvc'):
                print('PCH failed: the per-TU census runs without a PCH', flush=True)
                pch_flags = []
    else:
        hxx = write_pch_hxx(out, a.mode)
        pch_flags = None   # per TU below: only the TUs the MSVC build compiles with the PCH (/Yu + /FI cmake_pch.hxx) get the force-include
    if a.pch_only:
        print(open(os.path.join(out, 'pch.log')).read() if os.path.exists(os.path.join(out, 'pch.log')) else '')
        return
    def flags_for(c):
        if pch_flags is not None:
            return pch_flags
        if not any(x.startswith('/Yu') for x in c):
            return []
        return ['-include', hxx] if a.mode == 'native' else ['/FI' + hxx]
    work = [(a.mode, a.llvm, out, f, c, flags_for(c), a.force, extra) for f, c in tus]
    done = 0
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        for f, log, dt, rc in ex.map(run_one, work):
            done += 1
            if done % 50 == 0:
                print('%d/%d  %.0fs' % (done, len(work), time.time() - t0), flush=True)
    print('done %d TUs in %.0fs' % (len(work), time.time() - t0))


DIAG = re.compile(r'^(?P<file>/[^:(]+)[:(](?P<line>\d+)[:,](?P<col>\d+)\)?: (?P<sev>error|fatal error): (?P<msg>.*)$')


def cmd_report(a):
    out = os.path.abspath(a.out)
    rows = []
    for name in sorted(os.listdir(os.path.join(out, 'logs'))):
        txt = open(os.path.join(out, 'logs', name), errors='replace').read()
        head = txt.split('\n', 1)[0]
        m = re.match(r'# (.*?) rc=(-?\d+)', head)
        tu, rc = m.group(1), int(m.group(2))
        errs = []
        for line in txt.split('\n'):
            d = DIAG.match(line)
            if d:
                errs.append((d['file'], int(d['line']), d['msg']))
        rows.append((tu, rc, errs))
    with open(os.path.join(out, 'errors.tsv'), 'w') as o:
        o.write('tu\tfile\tline\tmsg\n')
        for tu, rc, errs in rows:
            for f, l, m in errs:
                o.write('%s\t%s\t%d\t%s\n' % (tu, f, l, m.replace('\t', ' ')))
    ok = sum(1 for _, rc, e in rows if rc == 0 and not e)
    print('TUs %d, clean %d, with errors %d, total diagnostics %d' % (len(rows), ok, len(rows) - ok, sum(len(e) for _, _, e in rows)))


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest='cmd', required=True)
    r = sp.add_parser('run')
    r.add_argument('--mode', choices=['msvc', 'native'], required=True)
    r.add_argument('--build-dir', default=os.path.join(REPO, 'build', 'StandaloneRelease'))
    r.add_argument('--out', required=True)
    r.add_argument('--llvm', default=LLVM_DEFAULT)
    r.add_argument('-j', '--jobs', type=int, default=6)
    r.add_argument('--only')
    r.add_argument('--extra', help='extra compiler flags (quoted string)')
    r.add_argument('--no-pch', dest='pch', action='store_false')
    r.add_argument('--force', action='store_true')
    r.add_argument('--pch-only', action='store_true', help='only (re)build the PCH over StdInc.h and print its errors')
    q = sp.add_parser('report')
    q.add_argument('--out', required=True)
    a = ap.parse_args()
    {'run': cmd_run, 'report': cmd_report}[a.cmd](a)


if __name__ == '__main__':
    main()
