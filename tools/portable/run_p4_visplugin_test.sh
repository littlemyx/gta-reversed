#!/bin/bash
# P4-S2 host test runner: tests/portable/p4_visplugin_test.cpp on the REAL text of the frame-visibility plugin (NOTSA_PORTABLE branches of source/game_sa/VisibilityPlugins.{h,cpp}) extracted into $OUT/visplugin.inc. `mutate`: variants must FAIL.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/p4_visplugin_test}"; mkdir -p "$OUT"
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"; [ -x "$CLANG" ] || CLANG=clang++
SDK=(); [ "$(uname)" = Darwin ] && SDK=(-isysroot "$(xcrun --show-sdk-path)")
T="$REPO/tests/portable"; G="$REPO/source/game_sa"
extract() { python3 - "$G" "$1" <<'PY'
import re, sys
g, out = sys.argv[1], sys.argv[2]
h = open(g + '/VisibilityPlugins.h').read(); c = open(g + '/VisibilityPlugins.cpp').read()
st = re.search(r'struct tFrameVisibilityPlugin \{.*?\n\};', h, re.S); assert st, 'struct'
parts = [st.group(0)]
for m in re.finditer(r'#define FRAMEPLG_HIERARCHYID\(frame\) \\\n[^\n]*\n', c): parts.append(m.group(0))
m = re.search(r'#ifdef NOTSA_PORTABLE\n#define FRAMEPLG_MODELINFO.*?#endif\n', c, re.S); assert m, 'modelinfo macro'; parts.append(m.group(0))
for name in ['FrameConstructor', 'FrameCopyConstructor', 'GetClumpModelInfo', 'GetFrameHierarchyId', 'SetClumpModelInfo', 'SetFrameHierarchyId']:
    m = re.search(r'^[^\n]*CVisibilityPlugins::' + name + r'\([^\n]*\{\n.*?\n\}\n', c, re.S | re.M); assert m, name; parts.append(m.group(0))
open(out, 'w').write('\n'.join(parts))
PY
}
build() { "$CLANG" "${SDK[@]}" -std=c++23 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -w -I"$1" -o "$2" "$T/p4_visplugin_test.cpp" 2>"$OUT/build.err" || { head -30 "$OUT/build.err"; return 1; }; }
mkdir -p "$OUT/base"; extract "$OUT/base/visplugin.inc" || exit 1
build "$OUT/base" "$OUT/t" || { echo "BUILD FAILED"; exit 1; }
rc=0; ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/t" || rc=1
[ "${1:-}" = "mutate" ] || { [ $rc = 0 ] && echo "RESULT P4_VISPLUGIN OK" || echo "RESULT P4_VISPLUGIN FAILED"; exit $rc; }
[ $rc = 0 ] || { echo "RESULT P4_VISPLUGIN FAILED (positive run)"; exit 1; }
bad=0; n=0
mut() { n=$((n+1)); d="$OUT/mut_$1"; mkdir -p "$d"
  python3 - "$OUT/base/visplugin.inc" "$d/visplugin.inc" "$2" "$3" <<'PY' || { echo "MUTANT $1: pattern not found"; bad=$((bad+1)); return; }
import sys; s=open(sys.argv[1]).read(); assert sys.argv[3] in s, 'pattern'; open(sys.argv[2],'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
  if ! build "$d" "$OUT/m_$1" >/dev/null 2>&1; then echo "MUTANT $1: BUILD ERROR"; bad=$((bad+1))
  elif ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/m_$1" >"$OUT/m_$1.log" 2>&1; then echo "MUTANT $1: SURVIVED"; bad=$((bad+1)); else echo "MUTANT $1: killed"; fi; }
mut noptr        'FRAMEPLG_MODELINFO(frame) = clumpModelInfo;' ''
mut trunc        'FRAMEPLG_MODELINFO(frame) = clumpModelInfo;' 'FRAMEPLG_MODELINFO(frame) = (CClumpModelInfo*)(uintptr_t)(uint32)(uintptr_t)clumpModelInfo;'
mut zeroid       'SetFrameHierarchyId(frame, id32 ? id32 : 1);' 'SetFrameHierarchyId(frame, id32);'
mut noclear      'FRAMEPLG_MODELINFO(frame) = nullptr; // one slot' '; // one slot'
mut copynoptr    'FRAMEPLG_MODELINFO(destFrame) = FRAMEPLG_MODELINFO(srcFrame);' ''
mut ctornoclear  'FRAMEPLG_MODELINFO(frame) = nullptr;
#endif
    return frame;' '
#endif
    return frame;'
mut idnotset     'SetFrameHierarchyId(frame, id32 ? id32 : 1);' '(void)id32;'
echo "mutants: $n, survivors/problems: $bad"
[ $bad = 0 ] && echo "RESULT P4_VISPLUGIN MUTATE OK" || { echo "RESULT P4_VISPLUGIN MUTATE FAILED"; exit 1; }
