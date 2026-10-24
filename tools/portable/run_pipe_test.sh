#!/bin/bash
# R-6 pipeline test (macOS arm64): tests/portable/rw_pipeline_gl_test.cpp renders synthetic SA-style geometry (texture orientation, texel order, prelit / material colour,
# lighting, alpha test / blend / z / cull / fog, the NVC lock-prelight mechanism, matfx env) and real models from gta3.img / player.img (buildings, skinned peds, a car) with
# RpAtomicRender into the framebuffer on librw's stock gl3 pipelines, reads the pixels back and checks coverage / colours / glGetError. PPM dumps in build/pipe_test/.
#  - same build recipe as run_txd_test.sh (shares its librw build in build/gl_smoke/librw); compiles the whole renderer-neutral RW shim (standalone/rw/*.cpp minus the excluded
#    D3D9 files) + portable/render/*.cpp, no game link; needs a display session; real-data part needs the Steam copy ($GTA)
#  - output: build/pipe_test/run.log, last line "PIPE RESULT OK|FAIL|SKIP"; env: GTA, NOTSA_PIPE_DUMP (dump dir)
# usage: tools/portable/run_pipe_test.sh
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-$REPO/build/pipe_test}"; mkdir -p "$OUT"
LIBOUT="${LIBOUT:-$REPO/build/gl_smoke}"; mkdir -p "$LIBOUT"
LLVM=/opt/homebrew/opt/llvm
SDL3=/opt/homebrew/opt/sdl3
S="$REPO/source"
LOW="taskpolicy -b nice -n 10"

# 1) librw (GL3 + SDL3)
if ! $LOW cmake -S "${LIBRW_SRC:-$REPO/vendor/librw}" -B "$LIBOUT/librw" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=$LLVM/bin/clang -DCMAKE_CXX_COMPILER=$LLVM/bin/clang++ \
     -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_PREFIX_PATH=/opt/homebrew -DLIBRW_PLATFORM=GL3 -DLIBRW_GL3_GFXLIB=SDL3 \
     -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF -DLIBRW_EXAMPLES=OFF >"$LIBOUT/librw_cfg.log" 2>&1; then tail -20 "$LIBOUT/librw_cfg.log"; echo "librw configure failed"; exit 1; fi
$LOW cmake --build "$LIBOUT/librw" >"$LIBOUT/librw_build.log" 2>&1 || { tail -30 "$LIBOUT/librw_build.log"; echo "librw build failed"; exit 1; }
LIBRW=$(find "$LIBOUT/librw" -name 'librw.a' | head -1)

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
FILES=("$REPO/tests/portable/rw_pipeline_gl_test.cpp" "$S/portable/render/pipelines_gl3.cpp" "$S/portable/render/pipelines_carenv_gl3.cpp" "$S/portable/render/texture_gl3.cpp" "$S/portable/render/engine_gl3.cpp" "$S/portable/render/platform_gl3.cpp"
       "$S/portable/render/camera_gl3.cpp" "$S/portable/render/raster_gl3.cpp" "$S/portable/render/im2d_im3d_gl3.cpp" "$S/portable/path_resolve.cpp")
# the renderer-neutral RW shim (everything of standalone/rw except the D3D9 fixed-function files that the portable build excludes, see PORTABLE_EXCLUDED_RENDER_FILES)
for f in "$S"/standalone/rw/*.cpp; do
  case "$(basename "$f")" in pipeline.cpp|pipeline_matfx.cpp|rwd3d_ff.cpp|pipeline_skin.cpp|pipeline_skin_cpu.cpp|skin_vs.cpp|im2d_im3d.cpp|platform.cpp|stream.cpp|rwtexdict.cpp) continue;; esac
  FILES+=("$f")
done
OBJS=()
for f in "${FILES[@]}"; do
  o="$OUT/$(basename "$f" .cpp).o"; OBJS+=("$o")
  # background-free incremental build: recompile only when the source (or one of the portable headers) is newer
  if [ -f "$o" ] && [ -z "$(find "$f" "$S/standalone/rw" "$S/portable/render" "$S/fakerw" "${LIBRW_SRC:-$REPO/vendor/librw}/src" -newer "$o" \( -name '*.cpp' -o -name '*.h' -o -name '*.inc' \) -print -quit)" ]; then continue; fi
  # game TUs (StdInc.h / PCH) are compiled with their exact PortableMac compile command (output redirected); the test itself with the reduced flags
  CMD=$(python3 - "$COMPDB" "$f" "$o" <<'PY'
import json, shlex, sys
cc = json.load(open(sys.argv[1])); f = sys.argv[2]; o = sys.argv[3]
e = next((x for x in cc if x['file'] == f), None)
if e is None or '#include "StdInc.h"' not in open(f).read(): sys.exit(0)
c = shlex.split(e['command']); out = []; i = 0
while i < len(c):
    if c[i] in ('-o', '-MF', '-MT'): i += 2; continue
    if c[i] == '-MD': i += 1; continue
    if c[i] == '-c': i += 2; continue
    out.append(c[i]); i += 1
out += ['-c', f, '-o', o]
print(' '.join(shlex.quote(x) for x in out))
PY
)
  if [ -n "$CMD" ]; then eval "$LOW $CMD" >"$o.log" 2>&1 || { head -40 "$o.log"; echo "COMPILE FAILED: $f"; exit 1; }
  else eval "$LOW $BASE $INC -w -g -O1 -c '$f' -o '$o'" >"$o.log" 2>&1 || { head -40 "$o.log"; echo "COMPILE FAILED: $f"; exit 1; }; fi
done
eval "$LOW $BASE -o '$OUT/rw_pipeline_gl_test' ${OBJS[*]} '$LIBRW' -L$SDL3/lib -lSDL3 -Wl,-rpath,$SDL3/lib -framework OpenGL -framework Cocoa -framework IOKit -L$LLVM/lib/c++ -Wl,-rpath,$LLVM/lib/c++" >"$OUT/link.log" 2>&1 \
  || { head -40 "$OUT/link.log"; echo "LINK FAILED"; exit 1; }

# 3) run (a display session is required); stderr is merged: librw prints shader compile errors there
"$OUT/rw_pipeline_gl_test" "$@" >"$OUT/run.log" 2>&1; rc=$?
cat "$OUT/run.log"
if grep -qE "Error in (vertex|fragment|program|.* shader)" "$OUT/run.log"; then echo "SHADER ERRORS in run.log"; rc=1; fi
grep -q "PIPE RESULT FAIL" "$OUT/run.log" && rc=1
echo "exit code $rc"
exit $rc
