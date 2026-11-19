"""sumorder_scan.py - find inline 3-term squared-sum sites (x87 fld st0/fmul st1/.. pattern) in functions of the exe; prints summation order per site.
usage: python allscan.py <hitcount.tsv> <exe> <repo> [mincount]   (rows: exe_addr, function, count, ...)"""
import subprocess,re,sys,csv
tsv,exe,repo=sys.argv[1:4]
mn=int(sys.argv[4]) if len(sys.argv)>4 else 1
PY=sys.executable
skip=re.compile(r'Audio|Hud|HUD|Font|Sprite|Render|Draw|Shadow|Corona|Fx|Particle|Skidmark|Vector::')
rows=[r for r in csv.DictReader(open(tsv,encoding='utf8'),delimiter='\t') if r['exe_addr']!='-' and int(r['count'])>=mn]
rows.sort(key=lambda r:-int(r['count']))
def ins(l):
    m=re.match(r'([0-9a-f]{8})\s+(.*)',l); return (m.group(1),re.sub(r'\s+',' ',m.group(2).strip())) if m else (None,l)
seen=set()
for r in rows:
    if skip.search(r['function']): continue
    for a in r['exe_addr'].split('|'):
        if a in seen: continue
        seen.add(a)
        out=subprocess.run([PY,'-X','utf8','-I','tools/standalone/exedis.py',a,'--exe',exe,'--repo',repo,'--max','8000'],capture_output=True,text=True,encoding='utf8').stdout.splitlines()
        L=[ins(l) for l in out]
        for i in range(len(L)-8):
            t=[x[1] for x in L[i:i+7]]
            if t[:7]==['fld st(0)','fmul st(1)','fld st(2)','fmul st(3)','faddp st(1)','fld st(3)','fmul st(4)']:
                lds=[]
                for k in range(i-1,max(0,i-25),-1):
                    s=L[k][1]
                    if s.startswith('fld ') and 'st(' not in s: lds.append(s[len('fld dword ptr '):])
                    elif s.startswith(('fsub ','fadd ','fmul ')): lds.append('~'+s)
                    if len(lds)>=3: break
                nxt=' / '.join(x[1] for x in L[i+7:i+16] if x[1].startswith(('fcom','fsqrt','test','fstp','j')))
                print(r['count'],r['function'],a,L[i][0],' ; '.join(lds),'|',nxt,flush=True)
