#!/usr/bin/env python3
# plink.py OUT.exe objdir [objdir...] : relink gta_reversed.exe from the shared build's objects, substituting objects found in the objdirs
import sys,re,os,subprocess
BD="/Users/Andrei.Mukhin/github/recomp/gta-reversed/build/StandaloneRelease"
out=sys.argv[1]; dirs=sys.argv[2:]
txt=open(BD+"/build.ninja").read()
m=re.search(r"^build bin/gta_reversed\.exe: \S+ ([^\n]*)\n((?:  [^\n]*\n)+)",txt,re.M)
ins=m.group(1).split(" | ")[0].split(" ")
v=dict(re.findall(r"^  (\w+) = (.*)$",m.group(2),re.M))
subs={}
for d in dirs:
    for f in os.listdir(d):
        if f.endswith(".obj"): subs[f]=os.path.join(d,f)
objs=[]; n=0
for o in ins:
    if not o.endswith(".obj"): continue
    mm=re.match(r"source/CMakeFiles/gta_reversed\.dir/(.*)\.obj$",o)
    if mm:
        flat=mm.group(1).replace("/","_")+".obj"
        if flat in subs: objs.append(subs[flat]); n+=1; continue
    objs.append(os.path.join(os.environ.get('OBJBASE',BD),o))
objs+= [v for k,v in subs.items() if k.startswith("extra_")]
print("substituted",n,"of",len(objs),flush=True)
rsp=os.path.join(os.path.dirname(out),"link.rsp")
open(rsp,"w").write("\n".join(objs)+"\n"+v["LINK_PATH"]+" "+v["LINK_LIBRARIES"]+"\n")
cmd="/Users/Andrei.Mukhin/tools/msvc/bin/x86/link /nologo @%s /out:%s /implib:%s.lib /pdb:%s.pdb %s"%(rsp,out,out,out,v["LINK_FLAGS"])
p=subprocess.run(cmd,shell=True,cwd=BD,capture_output=True,text=True)
print(p.returncode,(p.stdout+p.stderr)[-1500:])
