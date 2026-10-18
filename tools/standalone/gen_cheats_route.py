#!/usr/bin/env python3
"""VT-OVERRIDES: generate the scripted cheat scene for BOTH exes from one list: tools/standalone/cheats_route.txt (run build, soak.sh format) and tools/standalone/windows/cheats_scenes.txt (original exe, inputdrv).
Every cheat is typed (hash of the REVERSED code = the table entry), the script waits 3.5 s, the port logs `[probe] c<idx>_<CODE> ...` (InjProbe), the original is sampled by tools/standalone/windows/cheat_probe_exe.py
(the sample just before the next `timesCheated` increment = the state after that cheat). Risky cheats (peds attack, blow up cars, suicide) are last.
usage: python -X utf8 -I tools/standalone/gen_cheats_route.py
"""
import sys

# index in CCheat::m_aCheatHashKeys -> code (the code whose reversed CKeyGen hash is the table entry); found by tools/standalone/vtable_override_audit.py's companion check in .notes/reports/VT-OVERRIDES.md
CODES = {0: 'LXGIWYL', 1: 'KJKSZPJ', 2: 'UZUMYMW', 3: 'HESOYAM', 4: 'OSRBLHH', 5: 'ASNAEB', 6: 'AFZLLQLL', 7: 'ICIKPYH', 8: 'ALNSFMZO', 10: 'CFVFGMJ', 11: 'YSOHNUL', 12: 'PPGWJHT', 13: 'LIYOAAY',
         14: 'AJLOJYQY', 15: 'BAGOWPG', 16: 'FOOOXFT', 17: 'AIWPRTON', 18: 'OLDSPEEDDEMON', 19: 'JQNTDMH', 20: 'PDNEJOH', 21: 'VPJTQWV', 24: 'UBHYZHQ', 25: 'RZHSUEW', 26: 'CPKTNWT', 27: 'WHEELSONLYPLEASE',
         28: 'PGGOMOY', 29: 'GOODBYECRUELWORLD', 30: 'ZEIIVG', 31: 'YLTEICZ', 32: 'LLQPFBN', 33: 'IOWDLAC', 35: 'AFSNMSMW', 36: 'BTCDBCB', 37: 'BUFFMEUP', 38: 'KVGYZQK', 39: 'BLUESUEDESHOES',
         40: 'BGLUAWML', 41: 'LIFESABEACH', 42: 'ONLYHOMIESALLOWED', 43: 'BIFBUZZ', 44: 'AFPHULTL', 45: 'BEKKNQV', 47: 'GUSNHDE', 48: 'RIPAZHA', 49: 'CJPHONEHOME', 50: 'JUMPJET', 51: 'KGGGDKP',
         52: 'JCNRUAD', 53: 'SPEEDFREAK', 54: 'BSXSGGC', 55: 'NIGHTPROWLER', 56: 'OFVIAC', 57: 'MGHXYRM', 58: 'CWJXUOC', 60: 'KANGAROO', 62: 'CVWKXAM', 63: 'AIYPWZQP', 64: 'ROCKETMAN',
         66: 'BRINGITON', 67: 'STINGLIKEABEE', 69: 'STATEOFEMERGENCY', 70: 'PRIEBJ', 71: 'MUNASEF', 72: 'WANRLTW', 73: 'OUIQDMW', 75: 'FVTMNBZ', 76: 'SJMAHPE', 77: 'NOONECANSTOPUS', 78: 'ZSOXFSQ',
         79: 'WORSHIPME', 80: 'EHIBXQS', 81: 'VKYPQCF', 82: 'PROFESSIONALKILLER', 83: 'NATURALTALENT', 84: 'OHDUDE', 85: 'FOURWHEELFUN', 88: 'FLYINGTOSTUNT', 89: 'AGBDLCID'}
LAST = [14, 15, 40, 67, 26, 29]            # mayhem, everybody attacks, village people (rockets), riot, blow up cars, suicide
order = [i for i in sorted(CODES) if i not in LAST] + LAST
head = open('tools/standalone/audio_dense_route.txt', encoding='utf-8').read().split('until:control:60000')[0]
out = [head.rstrip('\n'), 'until:control:60000', 'mark:cheats_start', 'wait:1500', 'probe:start']
orig = open('tools/standalone/windows/audio_scenes.txt', encoding='utf-8').read().split('mark:foot_stand')[0]
oo = [orig.rstrip('\n'), 'mark:foot_stand', 'cap:30', 'wait:1500']
for i in order:
    c = CODES[i]
    out += ['mark:cheat_%d_%s' % (i, c)] + [x for ch in c for x in ('key:' + ch.lower(), 'wait:60')] + ['wait:3500', 'probe:c%d_%s' % (i, c)]
    oo += ['mark:cheat_%d_%s' % (i, c), 'cheat:' + c.lower(), 'wait:3500']
out += ['wait:2000', 'mark:quit_esc', 'key:escape', 'wait:3000']
oo += ['wait:2000']
open('tools/standalone/cheats_route.txt', 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
open('tools/standalone/windows/cheats_scenes.txt', 'w', encoding='utf-8', newline='\n').write('\n'.join(oo) + '\n')
print(len(order), 'cheats')
