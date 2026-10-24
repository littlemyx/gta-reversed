#!/bin/bash
# MAC-F4a differential test (tests/portable/f4a_lift_test.cpp): the verbatim x87 asm of every lifted site (MSVC x86 under Wine, golden vectors) vs the portable lifted code
# (clang, soft x87). usage: tools/portable/run_f4a_test.sh [N cases per site (20000)] [seed (1)]    env: OUT (work dir), CLANG, WINE_ENV (script that sets PATH/WINEPREFIX), SKIP_GOLDEN=1 (reuse $OUT/golden.bin)
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
N="${1:-20000}"; SEED="${2:-1}"
OUT="${OUT:-${TMPDIR:-/tmp}/f4a_test}"; mkdir -p "$OUT"
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"; [ -x "$CLANG" ] || CLANG=clang++
CORE="$REPO/source/game_sa/Core"
python3 "$REPO/tools/portable/x87lift_sites.py" harness "$OUT/f4a_sites.inc" || exit 1
if [ -z "${SKIP_GOLDEN:-}" ] || [ ! -f "$OUT/golden.bin" ]; then
  export PATH="$HOME/tools/Wine Staging.app/Contents/Resources/wine/bin:$HOME/tools/msvc/bin/x86:$PATH" WINEPREFIX="${WINEPREFIX:-$HOME/.wine-msvc}" WINEDEBUG=-all
  ( cd "$OUT" && rm -f f4a_ref.exe golden.bin && cl /nologo /O2 /arch:IA32 /EHsc /std:c++20 /DLIFT_ASM /I"$OUT" /I"$CORE" "$REPO/tests/portable/f4a_lift_test.cpp" /Fe:f4a_ref.exe > build_ref.log 2>&1 ) || { tail -30 "$OUT/build_ref.log"; echo "REF BUILD FAILED"; exit 1; }
  ( cd "$OUT" && wine ./f4a_ref.exe emit golden.bin "$N" "$SEED" > emit.log 2>&1 ) || { tail "$OUT/emit.log"; echo "REF RUN FAILED"; exit 1; }
fi
"$CLANG" -isysroot "$(xcrun --show-sdk-path)" -std=c++20 -O2 -DNOTSA_PORTABLE=1 -I"$OUT" -I"$CORE" -Wno-everything -o "$OUT/f4a_host" "$REPO/tests/portable/f4a_lift_test.cpp" 2> "$OUT/build_host.log" || { head -40 "$OUT/build_host.log"; echo "HOST BUILD FAILED"; exit 1; }
"$OUT/f4a_host" check "$OUT/golden.bin" "$N" "$SEED"

# MUTATE=1 (perl s/// = first match only): each mutation of a lifted .inc must make the test FAIL (valid case above, mutants here)
if [ -n "${MUTATE:-}" ]; then
  rc=0
  mut() { # name file sed-expr
    local d="$OUT/mut_$1"; rm -rf "$d"; mkdir -p "$d/x87_lifted"; perl -0pe "$3" "$CORE/x87_lifted/$2" > "$d/x87_lifted/$2"
    if cmp -s "$d/x87_lifted/$2" "$CORE/x87_lifted/$2"; then echo "MUTANT $1: sed changed nothing"; rc=1; return; fi
    "$CLANG" -isysroot "$(xcrun --show-sdk-path)" -std=c++20 -O2 -DNOTSA_PORTABLE=1 -I"$d" -I"$OUT" -I"$CORE" -Wno-everything -o "$d/t" "$REPO/tests/portable/f4a_lift_test.cpp" 2>/dev/null || { echo "MUTANT $1: build failed"; rc=1; return; }
    if "$d/t" check "$OUT/golden.bin" "$N" "$SEED" > "$d/log" 2>&1; then echo "MUTANT $1: SURVIVED (test did not notice)"; rc=1; else echo "MUTANT $1: killed ($(grep -c 'hard\|got' "$d/log") diagnostic lines)"; fi
  }
  mut rotx_mul_to_add Matrix_RotateX.inc 's/Op::Mul/Op::Add/'
  mut slerp_sub_to_subr Quaternion_Slerp.inc 's/Op::Sub,/Op::SubR,/'
  mut euler_branch Matrix_ConvertToEulerAngles.inc 's/if \(!x87.zf\) goto/if (x87.zf) goto/'
  mut getroll_xch Placeable_GetRoll.inc 's/x87\.f\.Xch\(1\);//'
  mut catan_patan_swap Quaternion_CalcThetaFromQuats.inc 's/x87\.f\.Xch\(1\);//'
  [ $rc -eq 0 ] && echo "ALL MUTANTS KILLED" || echo "MUTATION CHECK FAILED"
fi
