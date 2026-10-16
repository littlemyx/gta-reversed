#!/usr/bin/env python3
# bench.py OUTDIR [EXTRA_GAME_FLAGS...] : build game_oracle_test bench with the test TU from /tmp/perf/bench and the game TUs (Collision/General/Vector/Matrix/Quaternion/AnimBlendNode) compiled with extra flags
import sys,re,os,subprocess
BD="/Users/Andrei.Mukhin/github/recomp/gta-reversed/build/StandaloneRelease"
REPO="/Users/Andrei.Mukhin/github/recomp/gta-reversed"
out=sys.argv[1]; extra=sys.argv[2:]
os.makedirs(out,exist_ok=True)
txt=open(BD+"/build.ninja").read()
def rule(obj):
    m=re.search(r"^build %s: \S+ (\S+)[^\n]*\n((?:  [^\n]*\n)+)"%re.escape(obj),txt,re.M)
    return dict(re.findall(r"^  (\w+) = (.*)$",m.group(2),re.M)),m.group(1)
cl="/Users/Andrei.Mukhin/tools/msvc/bin/x86/cl"
PCHSRC=BD+"/source/CMakeFiles/game_oracle_test.dir/cmake_pch.cxx"
def fixpch(f): return re.sub(r"/Fp\S+",f"/Fp{out}/pch.pch",f)
objs={}
jobs=[]
vp,_=rule("source/CMakeFiles/game_oracle_test.dir/cmake_pch.cxx.obj")
pchcmd=f"{cl} /nologo /TP {vp['DEFINES']} {vp['INCLUDES']} {fixpch(vp['FLAGS']).replace('/MP','')} /Fo{out}/pch.obj /Fd{out}/ /FS -c {PCHSRC}"
if not os.path.exists(out+"/pch.pch"):
    p=subprocess.run(pchcmd,shell=True,cwd=BD,capture_output=True,text=True); print("pch",p.returncode,(p.stdout+p.stderr)[-300:],flush=True)
T="source/CMakeFiles/game_oracle_test.dir/"
test_obj=T+"__/tests/standalone/game_oracle_test.cpp.obj"
v,src=rule(test_obj)
jobs.append(("test",out+"/test.obj",f"{cl} /nologo /TP {v['DEFINES']} {v['INCLUDES']} {fixpch(v['FLAGS']).replace('/MP','')} /Fo{out}/test.obj /Fd{out}/ /FS -c /tmp/perf/bench/tests/standalone/game_oracle_test.cpp"))
for s in ["game_sa/Collision/Collision.cpp","game_sa/General.cpp","game_sa/Core/Vector.cpp","game_sa/Core/Matrix.cpp","game_sa/Core/Quaternion.cpp","game_sa/Animation/AnimBlendNode.cpp"]:
    o=T+s+".obj"; v,src=rule(o); fo=out+"/"+s.replace("/","_")+".obj"
    jobs.append((s,fo,f"{cl} /nologo /TP {v['DEFINES']} {v['INCLUDES']} {fixpch(v['FLAGS']).replace('/MP','')} {' '.join(extra)} /Fo{fo} /Fd{out}/ /FS -c {os.environ.get('COLL_SRC',src) if s.endswith('Collision.cpp') else src}"))
import concurrent.futures as cf
def run(j):
    p=subprocess.run(j[2],shell=True,cwd=BD,capture_output=True,text=True)
    errs=[l for l in (p.stdout+p.stderr).splitlines() if re.search(r"error [CL]\d+|fatal",l)]
    return j[0],p.returncode,errs[:3]
with cf.ThreadPoolExecutor(4) as ex:
    for r in ex.map(run,jobs): print(r,flush=True)
# link
n=re.search(r"^build source/game_oracle_test\.exe: \S+ ([^\n]*)\n((?:  [^\n]*\n)+)",txt,re.M)
ins=n.group(1).split(" | ")[0].split(" ")
lv=dict(re.findall(r"^  (\w+) = (.*)$",n.group(2),re.M))
new=[]
for o in ins:
    if o.endswith("game_oracle_test.cpp.obj"): new.append(out+"/test.obj")
    elif o.startswith(T+"game_sa/"): new.append(out+"/"+o[len(T):].replace("/","_"))
    elif o.endswith("cmake_pch.cxx.obj"): new.append(out+"/pch.obj")
    elif o.endswith(".obj"): new.append(os.path.join(BD,o))
rsp=out+"/link.rsp"
open(rsp,"w").write("\n".join(new)+"\n"+lv["LINK_PATH"]+" "+lv["LINK_LIBRARIES"]+"\n")
cmd=f"/Users/Andrei.Mukhin/tools/msvc/bin/x86/link /nologo @{rsp} /out:{out}/bench.exe /implib:{out}/bench.lib /pdb:{out}/bench.pdb {lv['LINK_FLAGS']}"
p=subprocess.run(cmd,shell=True,cwd=BD,capture_output=True,text=True)
print("link",p.returncode,(p.stdout+p.stderr)[-600:])
