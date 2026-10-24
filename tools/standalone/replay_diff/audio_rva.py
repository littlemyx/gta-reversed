"""Prints NOTSA_LOCKSTEP_AUDIO_RVA for the port (pin bit 128, audio RNG partition): "lo-hi,lo-hi,..." hex image offsets of the code of every object file built from a source under source/game_sa/Audio/,
taken from the linker map (symbols of objects whose basename is an audio source; a range runs from a symbol to the next code symbol).
usage: py -I audio_rva.py <repo> <gta_reversed.map>"""
import re, sys, glob, os
repo, mp = sys.argv[1], sys.argv[2]
aud = set()
for f in glob.glob(os.path.join(repo, 'source', 'game_sa', 'Audio', '**', '*.cpp'), recursive=True):
    aud.add(os.path.basename(f) + '.obj')
syms = []
for ln in open(mp, errors='replace'):
    t = ln.split()
    if len(t) >= 5 and t[0].startswith('0001:') and t[-2] == 'f':   # 'f' = function, 'f i' (inline comdats shared by many objects) are skipped
        try:
            va = int(t[2], 16)
            if va >= 0x401000: syms.append((va - 0x400000, t[-1]))
        except ValueError: pass
syms.sort()
rng = []
for i, (a, o) in enumerate(syms):
    if o in aud:
        b = syms[i + 1][0] if i + 1 < len(syms) else a + 16
        if rng and rng[-1][1] >= a: rng[-1][1] = max(rng[-1][1], b)
        else: rng.append([a, b])
print(','.join('%x-%x' % (a, b) for a, b in rng))
