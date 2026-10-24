#!/usr/bin/env python3
"""ftol_ab.py - Win32 no-op proof for source edits that must not change the MSVC build (MAC-FTOL1; same method as MAC-P1 / MAC-ANIM1).

A = a git worktree of BASE (default: the commit before the edits), B = this working tree. Every TU is compiled by the MSVC x86 cl.exe under wine with the flags of
build/StandaloneRelease/compile_commands.json (/O2 /arch:IA32 ...), WITHOUT the PCH (StdInc.cpp is force-included from the respective tree), then tools/standalone/obj_code_sig.py
compares the code sections + relocation names (NOTSA_SIG_NORM_STRINGS=1: the two trees live in different directories).

usage: ftol_ab.py --base <commit> [--scratch DIR] [-j N] <TU.cpp>...      (paths relative to the repo or absolute)
exit 0 when every TU is identical.
"""
import argparse
import concurrent.futures as cf
import json
import os
import re
import shlex
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CC = os.path.join(REPO, "build/StandaloneRelease/compile_commands.json")


# wine environment of the msvc-wine toolchain (see memory: gta-reversed-tooling-mac)
os.environ["PATH"] = os.path.expanduser("~/tools/Wine Staging.app/Contents/Resources/wine/bin") + ":" + os.path.expanduser("~/.local/bin") + ":" + os.environ["PATH"]
os.environ.setdefault("WINEPREFIX", os.path.expanduser("~/.wine-msvc"))
os.environ.setdefault("WINEDEBUG", "-all")


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def compile_tu(job):
    tree, tu_rel, outdir, tag = job
    cc = json.load(open(CC))
    ent = next((e for e in cc if e["file"].endswith("/" + tu_rel)), None)
    if ent is None:
        return tag, tu_rel, "", False, "no compile command in build/StandaloneRelease/compile_commands.json (TU not part of the Win32 build?)"
    args = shlex.split(ent["command"])
    out = []
    for a in args:
        if a.startswith(("/Yu", "/Fp", "/FI", "/Fo", "/Fd", "/MP")):
            continue
        if a == "-c" or a == "/FS":
            continue
        if a.startswith(REPO) and a.endswith(tu_rel):
            continue
        out.append(a.replace(REPO + "/source", tree + "/source"))
    # the compile database lists every -I / -external:I of REPO/build as is: keep those (generated headers), they do not differ between A and B
    pch = os.path.join(outdir, tag + "_pch.hxx")
    open(pch, "w").write('#include "%s/source/StdInc.cpp"\n' % tree)
    os.makedirs(os.path.join(outdir, tag), exist_ok=True)
    obj = os.path.join(outdir, tag, tu_rel.replace("/", "_") + ".obj")
    out += ["/FI" + pch, "/Fo" + obj, "/c", os.path.join(tree, tu_rel)]
    r = subprocess.run(out, capture_output=True, text=True, cwd=ent["directory"])
    ok = os.path.exists(obj) and r.returncode == 0
    return tag, tu_rel, obj, ok, (r.stdout + r.stderr)[-600:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--scratch", default="/private/tmp/claude-501/ftol_ab")
    ap.add_argument("-j", type=int, default=6)
    ap.add_argument("tus", nargs="+")
    a = ap.parse_args()
    a.tus = [t for x in a.tus for t in (open(x[1:]).read().split() if x.startswith("@") else [x])]      # @listfile
    os.makedirs(a.scratch, exist_ok=True)
    wt = os.path.join(a.scratch, "wtA")
    if not os.path.isdir(wt):
        r = sh(["git", "-C", REPO, "worktree", "add", "--detach", wt, a.base])
        if r.returncode:
            sys.exit(r.stderr)
    else:
        sh(["git", "-C", wt, "checkout", "-q", "--detach", a.base])
    tus = [os.path.relpath(os.path.abspath(t), REPO) if os.path.isabs(t) or os.path.exists(t) else t for t in a.tus]
    jobs = [(wt, t, a.scratch, "A") for t in tus] + [(REPO, t, a.scratch, "B") for t in tus]
    res = {}
    with cf.ThreadPoolExecutor(a.j) as ex:
        for tag, t, obj, ok, log in ex.map(compile_tu, jobs):
            res[(tag, t)] = (obj, ok, log)
    bad = 0
    sig = os.path.join(REPO, "tools/standalone/obj_code_sig.py")
    env = dict(os.environ, NOTSA_SIG_NORM_STRINGS="1")
    for t in tus:
        oa, oka, la = res[("A", t)]
        ob, okb, lb = res[("B", t)]
        if not (oka and okb):
            print("%-60s COMPILE FAILED A=%s B=%s\n%s\n%s" % (t, oka, okb, la if not oka else "", lb if not okb else ""))
            bad += 1
            continue
        ja, jb = oa + ".json", ob + ".json"
        sh([sys.executable, "-I", sig, "sig", ja, os.path.basename(oa)], env=env, cwd=os.path.dirname(oa))
        sh([sys.executable, "-I", sig, "sig", jb, os.path.basename(ob)], env=env, cwd=os.path.dirname(ob))
        r = sh([sys.executable, "-I", sig, "cmp", ja, jb], env=env)
        same = r.returncode == 0 and "same 1, different 0, only-in-a 0, only-in-b 0" in r.stdout
        # object sizes as a second witness
        print("%-60s %s  (A %d B %d bytes)%s" % (t, "IDENTICAL" if same else "DIFFERENT", os.path.getsize(oa), os.path.getsize(ob), "" if same else "\n" + r.stdout[-400:]))
        bad += 0 if same else 1
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
