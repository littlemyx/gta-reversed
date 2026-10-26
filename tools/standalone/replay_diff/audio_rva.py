"""Prints NOTSA_LOCKSTEP_AUDIO_RVA for the port (pin bit 128, audio RNG partition): "lo-hi,lo-hi,..." hex image offsets of the code of every object file built from a source under source/game_sa/Audio/,
taken from the linker map (symbols of objects whose basename is an audio source; a range runs from a symbol to the next code symbol).
usage: py -I audio_rva.py <repo> <gta_reversed.map> [<gta_reversed.exe>]
With the exe given, the map must be the one the exe was linked with (map header "Timestamp is" == the exe's PE TimeDateStamp): a stale or foreign map = exit 3 (RD-ENV: a missing/stale
map silently turns pin 128 off for the port leg and moves the equal prefix from 98 to 42 at the first footstep sound)."""
import re, sys, glob, os, struct
repo, mp = sys.argv[1], sys.argv[2]
if len(sys.argv) > 3:
    d = open(sys.argv[3], 'rb').read(4096); pe = struct.unpack_from('<I', d, 0x3c)[0]; exe_ts = struct.unpack_from('<I', d, pe + 8)[0]
    map_ts = None
    for ln in open(mp, errors='replace'):
        m = re.match(r'\s*Timestamp is ([0-9a-fA-F]+)', ln)
        if m: map_ts = int(m.group(1), 16); break
    if map_ts != exe_ts:
        sys.stderr.write('audio_rva: %s (timestamp %s) does not belong to %s (PE timestamp %08x)\n' % (mp, '%08x' % map_ts if map_ts is not None else 'none', sys.argv[3], exe_ts)); sys.exit(3)
aud = set()
for f in glob.glob(os.path.join(repo, 'source', 'game_sa', 'Audio', '**', '*.cpp'), recursive=True):
    aud.add(os.path.basename(f) + '.obj')
syms = []
for ln in open(mp, errors='replace'):
    t = ln.split()
    if len(t) >= 5 and t[0].startswith('0001:') and t[-2] in ('f', 'i') and t[-3 if t[-2] == 'i' else -2] == 'f':   # 'f' = function; 'f i' = inline comdat
        try:
            va = int(t[2], 16)
            # RD-AB-8: inline comdats (shared helpers such as CGeneral::GetRandomNumberInRange<int>, first instantiated in whatever object came first) must END a range and never belong to one,
            # otherwise a general helper placed behind an audio object's last function is taken for audio code; only the audio-specific ones (CAE... templates) count as audio
            if va >= 0x401000: syms.append((va - 0x400000, t[-1] if (t[-2] == 'f' or 'CAE' in t[1]) else '-inline-'))
        except ValueError: pass
syms.sort()
rng = []
for i, (a, o) in enumerate(syms):
    if o in aud:
        b = syms[i + 1][0] if i + 1 < len(syms) else a + 16
        if rng and rng[-1][1] >= a: rng[-1][1] = max(rng[-1][1], b)
        else: rng.append([a, b])
r = ','.join('%x-%x' % (a, b) for a, b in rng)
if not r: sys.stderr.write('audio_rva: no audio object ranges found in %s\n' % mp); sys.exit(4)
print(r)
