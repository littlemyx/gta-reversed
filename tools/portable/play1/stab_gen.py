#!/usr/bin/env python3
# MAC-STAB1: generates stab_soak{1,2,3}.txt (seeded mixes of walk / drive / far teleport hops / weather+time / combat, ~7.5 min script time each). usage: stab_gen.py (in this dir)
import random
hdr=open('vis1_tour.txt').read().split('cmd:snap:free_ready')[0]
LS=[(2495,-1665),(2100,-1790),(1550,-1675),(1960,-2200),(2800,-1100)]
SF=[(-2026,156),(-1550,-450),(-2700,-300),(-1950,300)]
LV=[(2020,1010),(1680,1450),(2400,2000),(300,900)]
CT=[(-100,-1200),(-2300,-1700),(-200,2300),(-1400,2200)]
ALL=LS+SF+LV+CT
def tp(r,spot=None):
    x,y=spot or r.choice(ALL)
    return "cmd:tpg:%d:%d\ncmd:health:5000\nwait:2500\n"%(x,y)
def walk(r):
    return tp(r)+"down:w\nwait:%d\ndown:space\nwait:300\nup:space\ndown:d\nwait:%d\nup:d\ndown:lshift\nwait:%d\nup:lshift\ndown:a\nwait:%d\nup:a\nwait:1000\nup:w\ncmd:playerstat\n"%(r.randint(2000,4000),r.randint(300,900),r.randint(3000,6000),r.randint(300,900))
def drive(r):
    veh=r.choice([411,402,445,521,448,462,490,596,599,560])
    s=tp(r)+"cmd:roadveh:%d\nwait:1500\nkey:return\nuntil:invehicle:10000\nwait:1000\ndown:w\n"%veh
    for i in range(r.randint(5,8)):
        k=r.choice('ad'); s+="down:%s\nwait:%d\nup:%s\nwait:%d\n"%(k,r.randint(300,900),k,r.randint(1500,3500))
    return s+"up:w\ndown:s\nwait:1200\nup:s\nkey:return\nuntil:onfoot:8000\nwait:800\ncmd:playerstat\n"
def hop(r):
    return "".join(tp(r) for i in range(r.randint(4,6)))
def wthr(r):
    return "".join("cmd:weather:%d\ncmd:hour:%d\nwait:%d\n"%(r.choice([0,1,2,3,4,5,7,8,9,10,11,12,13,14,15,16,17,18,19]),r.randint(0,23),r.randint(2500,5000)) for i in range(r.randint(4,6)))
def combat(r):
    return tp(r)+"cmd:give:%d:500\nwait:800\ncmd:spawnmany:%d:%d:0\nwait:1500\ndown:lctrl\nwait:%d\nup:lctrl\ncmd:wanted:%d\nwait:%d\ncmd:shoot:4:30\nwait:1500\ncmd:killall:30:3:0:25\nwait:%d\ncmd:wanted:0\ncmd:poolstat\n"%(r.choice([30,31,28,22,24,25]),r.randint(6,12),r.randint(5,9),r.randint(2000,4000),r.randint(2,6),r.randint(3000,6000),r.randint(2000,4000))
blocks=[walk,drive,hop,wthr,combat]
for n,seed in enumerate((11,22,33),1):
    r=random.Random(seed)
    s="# MAC-STAB1 soak %d: seeded mix of walk / drive / far teleport hops / weather+time / combat, ~7.5 min script time, ends with cmd:quit (shutdown path)\n"%n+hdr+"cmd:snap:free_ready\ncmd:health:5000\ncmd:wanted:0\n"
    t=0; k=0
    while True:
        b=blocks[k%len(blocks)] if k<5 else r.choice(blocks)
        part=b(r); k+=1
        t+=sum(int(l.split(':')[1]) for l in part.splitlines() if l.startswith('wait:'))+1000
        s+="# block %d %s\n"%(k,b.__name__)+part+"cmd:poolstat\n"
        if k%6==0: s+="mark:soak_%d\n"%k
        if t>=450000: break
    s+="cmd:playerstat\ncmd:poolstat\nmark:soak_end\nwait:500\ncmd:quit\n"
    open('stab_soak%d.txt'%n,'w').write(s)
