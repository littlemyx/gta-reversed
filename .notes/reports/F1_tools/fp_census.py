import re,os,sys,struct,collections,json
root='source'
skip=('source/standalone','source/fakerw','source/game_sa/RenderWare/rw')
lit=re.compile(r'(?<![\w.])(\d+\.\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)(f|F)?(?![\w.])')
def isfloat_exact(s):
    d=float(s)
    return struct.unpack('<f',struct.pack('<f',d))[0]==d
per=collections.defaultdict(lambda: collections.Counter())
for dp,dn,fn in os.walk(root):
    if dp.startswith(skip): continue
    for f in fn:
        if not f.endswith(('.cpp','.h','.hpp','.inl')): continue
        p=os.path.join(dp,f)
        try: t=open(p,errors='replace').read()
        except: continue
        # strip comments
        t2=re.sub(r'//[^\n]*','',t); t2=re.sub(r'/\*.*?\*/','',t2,flags=re.S)
        c=per[p]
        c['x87']=len(re.findall(r'\bx87::(sin|cos|tan|atan2|asin|acos|atan|sqrt|log10|log2|ln|sincos)\b',t2))
        c['dcast']=len(re.findall(r'\(double\)',t2))
        c['ddecl']=len(re.findall(r'\bdouble\s+[A-Za-z_]',t2))
        c['stdmath']=len(re.findall(r'\bstd::(sin|cos|tan|atan2|atan|asin|acos|sqrt|pow|floor|ceil|exp|log|log10|fmod|modf|round|trunc|hypot|abs)\b',t2))
        c['sinf']=len(re.findall(r'\b(sinf|cosf|tanf|atan2f|sqrtf|powf|floorf|ceilf|fabsf|fmodf)\b',t2))
        c['ftol']=len(re.findall(r'\b(Ftol|ftol|_ftol2)\w*',t2))
        c['recip']=len(re.findall(r'\bExeRecip',t2))
        n=0;nx=0
        for m in lit.finditer(t2):
            s=m.group(1); fl=m.group(2)
            if fl: continue  # float literal
            n+=1
            try:
                if not isfloat_exact(s): nx+=1
            except: pass
        c['dlit']=n; c['dlit_inexact']=nx
        c['asm']=len(re.findall(r'\b__asm\b',t2)); c['naked']=len(re.findall(r'__declspec\(naked\)',t2))
tot=collections.Counter()
for p,c in per.items(): tot.update(c)
print(json.dumps(tot,indent=1))
rows=sorted(per.items(),key=lambda kv:-(kv[1]['x87']*3+kv[1]['dcast']+kv[1]['ddecl']))
json.dump({p:dict(c) for p,c in per.items()},open(sys.argv[1],'w'))
for p,c in rows[:45]: print(p.replace('source/game_sa/',''),dict((k,v) for k,v in c.items() if v))
print('files with x87:',sum(1 for c in per.values() if c['x87']),' with dcast/ddecl:',sum(1 for c in per.values() if c['dcast'] or c['ddecl']))
