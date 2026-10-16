#!/usr/bin/env python3
# pcc.py OUTDIR EXTRA_FLAGS -- src1 src2 ... : compile sources (relative to repo/source) with the flags of build/StandaloneRelease/build.ninja into OUTDIR/<flat>.obj
import sys,re,os,subprocess,shlex
BD="/Users/Andrei.Mukhin/github/recomp/gta-reversed/build/StandaloneRelease"
REPO="/Users/Andrei.Mukhin/github/recomp/gta-reversed"
out=sys.argv[1]; i=sys.argv.index("--"); extra=sys.argv[2:i]; srcs=sys.argv[i+1:]
txt=open(BD+"/build.ninja").read()
cmds=[]
for s0 in srcs:
    if s0.startswith("/"):
        abspath,s=s0.split("=",1)   # /abs/path.cpp=game_sa/Stand.cpp (rule donor and object name)
    else:
        abspath=REPO+"/source/"+s0; s=s0
    obj="source/CMakeFiles/gta_reversed.dir/%s.obj"%s
    m=re.search(r"^build %s: \S+ (\S+)[^\n]*\n((?:  [^\n]*\n)+)"%re.escape(obj),txt,re.M)
    if not m: print("no rule",s); continue
    v=dict(re.findall(r"^  (\w+) = (.*)$",m.group(2),re.M))
    fo=os.path.join(out,s.replace("/","_")+".obj")
    line="%s /nologo /TP %s %s %s %s /Fo%s /Fd%s/ /FS -c %s"%("/Users/Andrei.Mukhin/tools/msvc/bin/x86/cl",v["DEFINES"],v["INCLUDES"],v["FLAGS"].replace("/MP",""),' '.join(extra),fo,out,abspath)
    cmds.append((s,fo,line))
os.makedirs(out,exist_ok=True)
import concurrent.futures as cf
def run(c):
    s,fo,line=c
    p=subprocess.run(line,shell=True,cwd=BD,capture_output=True,text=True)
    errs=[l for l in (p.stdout+p.stderr).splitlines() if re.search(r"error [CL]\d+|fatal",l)]
    return s,fo,p.returncode,errs
J=int(os.environ.get("J","4"))
with cf.ThreadPoolExecutor(J) as ex:
    for s,fo,rc,errs in ex.map(run,cmds):
        print(("OK  " if rc==0 else "FAIL"),s,fo if rc==0 else errs[:3],flush=True)
