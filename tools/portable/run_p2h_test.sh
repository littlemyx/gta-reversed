#!/bin/bash
# P-2h host test (not wired in CMake; MAC-W0 owns CMake): tests/portable/p2h_pathnode_test.cpp
#  - goldens (CPathNode_File 0x1C: basic / extreme / chain) come from tools/portable/gen_x86_golden.py (independent of the code under test)
#  - REAL FileRecords/PathNodeFile.hpp, FileRecord.hpp; `class CPathNode` .. `// P-2h END` (class + FileRecord<CPathNode> + notsa::LoadPathNodesFromFile) cut verbatim from PathFind.h; the NOTSA_PORTABLE block of
#    LoadPathFindData cut verbatim from PathFind.cpp (compiled around a memory RwStream)
#  - REAL DATA: all 64 data/paths/NODES*.DAT of the Steam copy (never copied/committed): header numNodes == veh+ped, file_size == 20 + 28n + 14 links + 8 addrs + 1152, every node decoded through the loader
#    (areaId/nodeId == index, bit-fields, link ranges/targets); its STATS line must equal the independent Python census tools/portable/p2h_scan_nodes.py
#  - built with ASan + UBSan; `run_p2h_test.sh mutate` additionally builds source-mutated copies (in $OUT/mut) and demands that the test FAILS on each
# Compile flags (-D set, std, arch) come from build/PortableMac/compile_commands.json.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-${TMPDIR:-/tmp}/p2h_test}"; mkdir -p "$OUT"
GTA="${GTA:-$HOME/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas}"
export P2H_NODES_DIR="${P2H_NODES_DIR:-$GTA/data/paths}"
[ -f "$P2H_NODES_DIR/NODES0.DAT" ] || { echo "NOTE: $P2H_NODES_DIR/NODES0.DAT missing: real-data part will be skipped"; unset P2H_NODES_DIR; }
{ echo '#include <cstdint>'; python3 "$REPO/tools/portable/gen_x86_golden.py" cpp CPathNode_File; } > "$OUT/p2h_golden.inc" || exit 1
BASE=$(python3 - "$COMPDB" <<'PY'
import json, shlex, sys
cc = json.load(open(sys.argv[1]))
e = next(x for x in cc if x['file'].endswith('source/game_sa/Pickups.cpp'))
c = shlex.split(e['command']); keep = [c[0]]; i = 1
while i < len(c):
    a = c[i]
    if a in ('-arch', '-isysroot'): keep += [a, c[i+1]]; i += 2; continue
    if a.startswith('-D') or a.startswith('-std=') or a.startswith('-stdlib=') or a in ('-fms-extensions', '-mms-bitfields', '-fsigned-char', '-fwrapv', '-fno-delayed-template-parsing'): keep.append(a)
    i += 1
print(' '.join(shlex.quote(k) for k in keep))
PY
)
S="$REPO/source"
gen() { # $1 = work dir: cuts of the real sources
python3 - "$REPO" "$1" <<'PY'
import sys
repo, out = sys.argv[1], sys.argv[2]
h = open(repo + '/source/game_sa/PathFind.h').read()
a = h.index('class CPathNode {'); b = h.index('// P-2h END', a)
open(out + '/p2h_cut.h', 'w').write('#pragma once\n' + h[a:b + len('// P-2h END')] + '\n')
c = open(repo + '/source/game_sa/PathFind.cpp').read()
a = c.index('        // P-2h: the file stride'); b = c.index('#else', a)
open(out + '/p2h_load_block.inc', 'w').write(c[a:b])
PY
}
build() { # $1 = work dir, $2 = extra include dir (mutated copy) or "", $3 = exe
  local MUT=""; [ -n "$2" ] && MUT="-I'$2'"
  eval "nice -n 10 $BASE -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer $MUT -I'$1' -I'$REPO/tests/portable/p2c_stubs' -I$S/game_sa/Core -I$S/game_sa -I$S/game_sa/Enums -I$S -I'$OUT' -w -DREPO_DIR=$REPO -o '$3' '$REPO/tests/portable/p2h_pathnode_test.cpp'" 2>"$3.err"
}
export ASAN_OPTIONS=detect_leaks=0:symbolize=0
gen "$OUT" || exit 1
build "$OUT" "" "$OUT/p2h_pathnode_test" || { head -40 "$OUT/p2h_pathnode_test.err"; echo "BUILD FAILED"; exit 1; }
"$OUT/p2h_pathnode_test" | tee "$OUT/run.log"; rc=${PIPESTATUS[0]}
if [ -n "${P2H_NODES_DIR:-}" ]; then
  want=$(python3 -I "$REPO/tools/portable/p2h_scan_nodes.py" "$P2H_NODES_DIR")
  got=$(grep '^STATS ' "$OUT/run.log")
  if [ "$want" == "$got" ]; then echo "CENSUS MATCH (python == C++): $(echo "$got" | cut -c1-120)..."; else echo "CENSUS MISMATCH"; echo " python: $want"; echo " c++   : $got"; rc=1; fi
fi
[ $rc -eq 0 ] && echo "RESULT P2H OK" || echo "RESULT P2H FAIL (rc=$rc)"
if [ "${1:-}" == "mutate" ]; then
  mrc=0
  mutate() { # name, file kind (cut|hdr|blk), sed expr
    local D="$OUT/mut/$1"; rm -rf "$D"; mkdir -p "$D/FileRecords"
    cp "$OUT/p2h_cut.h" "$OUT/p2h_load_block.inc" "$D/"; cp "$S/game_sa/FileRecords/PathNodeFile.hpp" "$D/FileRecords/"
    local F
    case $2 in cut) F="$D/p2h_cut.h";; hdr) F="$D/FileRecords/PathNodeFile.hpp";; blk) F="$D/p2h_load_block.inc";; esac
    local before; before=$(cksum < "$F")
    sed -i.orig -e "$3" "$F"
    if [ "$before" == "$(cksum < "$F")" ]; then echo "MUTANT $1: sed did not change anything"; mrc=1; return; fi
    build "$D" "$D" "$D/t" || { echo "MUTANT $1: build failed"; head -5 "$D/t.err"; mrc=1; return; }
    if ( "$D/t" >"$D/run.log" 2>&1 ) 2>/dev/null; then echo "MUTANT $1: SURVIVED (test passed) <-- BAD"; mrc=1; else echo "MUTANT $1: caught ($(grep -c -E 'FAIL|runtime error|ERROR: AddressSanitizer' "$D/run.log") signals)"; fi
  }
  mutate keep-next      cut 's|n.m_next = nullptr;|(void)0;|'
  mutate keep-prev      cut 's|n.m_prev = nullptr;|(void)0;|'
  mutate pos-swap       cut 's|n.m_vPos = CompressedLargeVector{ f.pos\[0\], f.pos\[1\], f.pos\[2\] };|n.m_vPos = CompressedLargeVector{ f.pos[1], f.pos[0], f.pos[2] };|'
  mutate dist-lost      cut 's|n.m_totalDistFromOrigin = f.totalDistFromOrigin;|(void)0;|'
  mutate base-lost      cut 's|n.m_wBaseLinkId = f.baseLinkId;|(void)0;|'
  mutate area-lost      cut 's|n.m_wAreaId     = f.areaId;|(void)0;|'
  mutate node-lost      cut 's|n.m_wNodeId     = f.nodeId;|(void)0;|'
  mutate width-lost     cut 's|n.m_nPathWidth  = f.pathWidth;|(void)0;|'
  mutate flood-lost     cut 's|n.m_nFloodFill  = f.floodFill;|(void)0;|'
  mutate numlinks-mask  cut 's|n.m_nNumLinks                = (w >> kCPathNodeNumLinksShift) \& kCPathNodeNumLinksMask;|n.m_nNumLinks = (w >> kCPathNodeNumLinksShift) \& 7;|'
  mutate deadend-bit    cut 's|n.m_onDeadEnd                = (w >> kCPathNodeOnDeadEndBit) \& 1;|n.m_onDeadEnd = (w >> kCPathNodeIsSwitchedOffBit) \& 1;|'
  mutate highway-lost   cut 's|n.m_bHighway                 = (w >> kCPathNodeHighwayBit) \& 1;|(void)0;|'
  mutate spawn-shift    cut 's|n.m_nSpawnProbability        = (w >> kCPathNodeSpawnProbabilityShift)|n.m_nSpawnProbability = (w >> kCPathNodeBehaviourTypeShift)|'
  mutate behav-lost     cut 's|n.m_nBehaviourType           = (w >> kCPathNodeBehaviourTypeShift) \& kCPathNodeBehaviourTypeMask;|(void)0;|'
  mutate tofile-flags   cut 's/| (uint32_t)n.unk3 << kCPathNodeUnk3Bit | (uint32_t)n.unk4 << kCPathNodeUnk4Bit/| (uint32_t)n.unk3 << kCPathNodeUnk3Bit/'
  mutate tofile-pos     cut 's|f.pos\[2\] = n.m_vPos.z.Raw();|f.pos[2] = 0;|'
  mutate batch-skip     cut 's|done += n;|done += n; if (n == kBatch) ++done;|'
  mutate stride-mem     cut 's|read((void\*)batch, sizeof(CPathNode_File) \* n);|read((void*)batch, sizeof(CPathNode) * n);|'
  mutate bit-order      hdr 's|kCPathNodeSwitchedOffOriginalBit = 8, |kCPathNodeSwitchedOffOriginalBit = 9, |'
  mutate shift-spawn    hdr 's|kCPathNodeSpawnProbabilityShift = 16|kCPathNodeSpawnProbabilityShift = 17|'
  mutate use-mem-read   blk 's|(uint32)bytes|(uint32)bytes + 4|'
  [ $mrc -eq 0 ] && echo "RESULT P2H MUTANTS OK" || { echo "RESULT P2H MUTANTS FAIL"; rc=1; }
fi
exit $rc
