#!/bin/bash
# R-3a GL smoke test (macOS arm64): tests/portable/gl_smoke.cpp = a window like WinMain's, librw GL3 engine open/start through the game's RW API (engine.cpp portable
# view + portable/render/{engine_gl3,platform_gl3}.cpp), clear + Im2D + present, GL/stencil/depth facts, exit 0.
#  - builds vendor/librw (GL3 + SDL3, Homebrew SDL3) into build/gl_smoke/librw, compiles the shim files with the PortableMac flags (-D set, std, arch, from compile_commands.json)
#  - no game PCH, no game link: only fakerw + librw + SDL3 + the CRT
#  - needs a display session (WindowServer). Output: build/gl_smoke/run.log ; last line "SMOKE RESULT OK" / "SMOKE RESULT FAIL ..."
# usage: tools/portable/run_gl_smoke.sh [--frames N]
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-$REPO/build/gl_smoke}"; mkdir -p "$OUT"
LLVM=/opt/homebrew/opt/llvm
SDL3=/opt/homebrew/opt/sdl3
S="$REPO/source"
LOW="taskpolicy -b nice -n 10"

# 1) librw (GL3 + SDL3)
if ! $LOW cmake -S "${LIBRW_SRC:-$REPO/vendor/librw}" -B "$OUT/librw" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=$LLVM/bin/clang -DCMAKE_CXX_COMPILER=$LLVM/bin/clang++ \
     -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_PREFIX_PATH=/opt/homebrew -DLIBRW_PLATFORM=GL3 -DLIBRW_GL3_GFXLIB=SDL3 \
     -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF -DLIBRW_EXAMPLES=OFF >"$OUT/librw_cfg.log" 2>&1; then tail -20 "$OUT/librw_cfg.log"; echo "librw configure failed"; exit 1; fi
$LOW cmake --build "$OUT/librw" >"$OUT/librw_build.log" 2>&1 || { tail -30 "$OUT/librw_build.log"; echo "librw build failed"; exit 1; }
LIBRW=$(find "$OUT/librw" -name 'librw.a' | head -1)

# 2) compile flags of the game target (defines, std, arch, sysroot); includes are given explicitly
BASE=$(python3 - "$COMPDB" <<'PY'
import json, shlex, sys
cc = json.load(open(sys.argv[1]))
e = next(x for x in cc if x['file'].endswith('source/standalone/rw/engine.cpp'))
c = shlex.split(e['command']); keep = [c[0]]; i = 1
while i < len(c):
    a = c[i]
    if a in ('-arch', '-isysroot'): keep += [a, c[i+1]]; i += 2; continue
    if a.startswith('-D') or a.startswith('-std=') or a.startswith('-stdlib=') or a in ('-fms-extensions', '-mms-bitfields', '-fsigned-char', '-fwrapv', '-fno-delayed-template-parsing', '-fno-strict-aliasing', '-ffp-contract=off'): keep.append(a)
    i += 1
print(' '.join(shlex.quote(k) for k in keep))
PY
)
[ -n "$BASE" ] || { echo "no compile flags (build/PortableMac/compile_commands.json missing?)"; exit 1; }
INC="-I$S/fakerw -I$S -I$S/portable/winshim -I$S/game_sa -I$S/game_sa/RenderWare -I$S/game_sa/RenderWare/rw -I${LIBRW_SRC:-$REPO/vendor/librw} -isystem $SDL3/include"
FILES=("$REPO/tests/portable/gl_smoke.cpp" "$S/standalone/rw/engine.cpp" "$S/standalone/rw/rwglobals.cpp" "$S/portable/render/engine_gl3.cpp" "$S/portable/render/platform_gl3.cpp"
       "$S/standalone/rw/camera.cpp" "$S/standalone/rw/raster.cpp" "$S/standalone/rw/renderstate.cpp" "$S/portable/render/camera_gl3.cpp" "$S/portable/render/raster_gl3.cpp" "$S/portable/render/im2d_im3d_gl3.cpp" "$S/portable/path_resolve.cpp")
OBJS=()
for f in "${FILES[@]}"; do
  o="$OUT/$(basename "$f" .cpp).o"; OBJS+=("$o")
  # background-free incremental build: recompile only when the source (or one of the portable headers) is newer
  if [ -f "$o" ] && [ -z "$(find "$f" "$S/standalone/rw" "$S/portable/render" "$S/fakerw" "${LIBRW_SRC:-$REPO/vendor/librw}/src" -newer "$o" \( -name '*.cpp' -o -name '*.h' -o -name '*.inc' \) -print -quit)" ]; then continue; fi
  eval "$LOW $BASE $INC -w -g -c '$f' -o '$o'" >"$o.log" 2>&1 || { head -40 "$o.log"; echo "COMPILE FAILED: $f"; exit 1; }
done
eval "$LOW $BASE -o '$OUT/gl_smoke' ${OBJS[*]} '$LIBRW' -L$SDL3/lib -lSDL3 -Wl,-rpath,$SDL3/lib -framework OpenGL -framework Cocoa -framework IOKit -L$LLVM/lib/c++ -Wl,-rpath,$LLVM/lib/c++" >"$OUT/link.log" 2>&1 \
  || { head -40 "$OUT/link.log"; echo "LINK FAILED"; exit 1; }

# 3) run (a display session is required); stderr is merged: librw prints shader compile errors there
"$OUT/gl_smoke" "$@" >"$OUT/run.log" 2>&1; rc=$?
cat "$OUT/run.log"
if grep -qE "Error in (vertex|fragment|program|.* shader)" "$OUT/run.log"; then echo "SHADER ERRORS in run.log"; rc=1; fi
echo "exit code $rc"
exit $rc
