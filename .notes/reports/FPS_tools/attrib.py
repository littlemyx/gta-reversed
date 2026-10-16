import sys,os,re,statistics,collections
sys.argv=[sys.argv[0]]+sys.argv[1:]
import analyze as A
names=["Present","Clear","Draw","Create(res+shader)","Lock/Unlock","Update/Stretch"]
def run(runid,scenesel=("foot_stand","walk_a","walk_b")):
    ts=A.load(runid); R=A.RECS
    out=[]
    for name,w0,w1,cpu in A.scenes(runid):
        if name not in scenesel: continue
        fr=[]
        for i in range(1,len(R)):
            if w0<=R[i][0]<=w1: fr.append((R[i][0]-R[i-1][0],R[i]))
        if len(fr)<10: continue
        hit=[f for f in fr if f[0]>50]; ok=[f for f in fr if f[0]<=50]
        def mean(fs,k): return sum(f[1][1+k] for f in fs)/len(fs) if fs else 0
        row=dict(name=name,n=len(fr),nh=len(hit))
        if hit:
            row['hperiod']=sum(f[0] for f in hit)/len(hit)
            row['hb']=[mean(hit,k) for k in range(6)]
            row['h_other']=row['hperiod']-sum(row['hb'])
            row['hdraws']=sum(f[1][9] for f in hit)/len(hit)
        row['okperiod']=sum(f[0] for f in ok)/max(1,len(ok)); row['okb']=[mean(ok,k) for k in range(6)]; row['ok_other']=row['okperiod']-sum(row['okb']); row['okdraws']=sum(f[1][9] for f in ok)/max(1,len(ok))
        ht=[f[1][0] for f in hit]; iv=[round((ht[i+1]-ht[i])/1000.0*2)/2 for i in range(len(ht)-1) if ht[i+1]-ht[i]>200]
        row['iv_top']=collections.Counter(iv).most_common(3)
        out.append(row)
    return out
if __name__=="__main__":
    for r in sys.argv[1:]:
        print("==",r)
        for row in run(r):
            print("  %-10s n=%d hitches>50ms=%d  ok-frames: period %.1f ms [D3D %.1f (present %.1f draw %.1f lock %.1f create %.1f) other %.1f] draws/frame %.0f"%(row['name'],row['n'],row['nh'],row['okperiod'],sum(row['okb']),row['okb'][0],row['okb'][2],row['okb'][4],row['okb'][3],row['ok_other'],row['okdraws']))
            if row['nh']:
                hb=row['hb']; print("             hitch frames: mean %.0f ms [present %.0f clear %.0f draw %.0f create %.0f lock %.0f update %.0f | non-D3D (game logic+streaming+audio+descheduling) %.0f]  draws/frame %.0f  gaps between hitches %s"%(row['hperiod'],hb[0],hb[1],hb[2],hb[3],hb[4],hb[5],row['h_other'],row['hdraws'],row['iv_top']))
