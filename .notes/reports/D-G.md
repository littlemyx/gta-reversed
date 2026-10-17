# D-G: build and clean-clone proof (Windows, commits ac8d234e, 64689575)

## 1. G1 changes
| what | where |
|---|---|
| `GTASA_ORIGINAL_EXE` deleted from `StandaloneRelease` (and so `StandaloneClangCL`) and `StandaloneRW`; the CMake variable is now `GTASA_ORACLE_EXE` (dev only, FILEPATH, empty by default, must exist if given). Only preset `StandaloneDetached` sets it (`${sourceDir}/gta_sa_compact.exe`) | `CMakePresets.json`, `source/CMakeLists.txt` |
| extract custom command, `gta_reversed_exe_data` target, POST_BUILD copy: only inside `if(GTASA_IMAGE_DIAG)`; `GTASA_IMAGE_DIAG` without `GTASA_ORACLE_EXE` is a clear FATAL_ERROR ("dev-only configuration ... the run exe does not need any exe") | `source/CMakeLists.txt` |
| python is no longer a configure error for the run exe (find_program soft; FATAL only for IMAGE_DIAG; the oracle test targets use it for the placeholder object). `unicorn`/`capstone` are only for dev tools | `source/CMakeLists.txt`, docs |
| 23 exe-oracle tests: missing `RW_EXE_ORACLE` -> `return 77` (was `return 0 "skipped"` / `return 2`); the 2 partial ones (`rw_math_stream_test`, `rw_skin_vs_test`) keep running their non-oracle sections | `tests/standalone/*_test.cpp` (one line each) |
| `run_all_tests.sh`: `GTA_EXE` optional, an inherited `RW_EXE_ORACLE` is `unset`, exit 77 -> `SKIP(no_exe)` (no retry, not ok, not a failure, exit code unaffected; if the exe WAS given and a test still exits 77 it is `EXIT77` = failure), no `-DGTASA_ORIGINAL_EXE`, summary line `N tests: X ok, Y SKIP(no exe), Z other`, new `BUILD_ONLY=1` | `tools/standalone/run_all_tests.sh` |
| `soak.sh`: writes `$OUT/datadir_inventory.txt` (sha256+size of the exe/dll/bin files of the run dir), deletes `gta_sa.exe / gta_sa_compact.exe / original_data.* / data_pointers.bin` from the data dir and exits 2 if one is still there (unless `SOAK_IMAGE=1`) | `tools/standalone/soak.sh` |
| `census.py` (clear error without `bin/original_data.bin`), `perf_datadir.sh` (orig kind needs the exe, says so), `play.cmd` (comment), `ImageLoad.cpp` message, docs `STANDALONE.md`, `WINDOWS_SETUP.md`, `STANDALONE_FIDELITY.md` | |
| `hostenv.sh`: nothing to change (no exe reference) | |

Negative checks (clean clone, no exe): `cmake --preset StandaloneDetached` -> "GTASA_ORACLE_EXE does not exist: .../gta_sa_compact.exe"; `-DGTASA_IMAGE_DIAG=ON -DGTASA_ORACLE_EXE=` -> "GTASA_IMAGE_DIAG is a dev-only configuration and needs -DGTASA_ORACLE_EXE". Main checkout: `build/StandaloneDetached` reconfigured and rebuilt with the exe (extract step ran, rc 0).

## 2. G2: clean clone without any exe
Setup: `git clone --recurse-submodules -b reverse/interior_c https://github.com/littlemyx/gta-reversed.git C:/gta-scratch/g2` at ac8d234e (3,003 files, vendor/librw f358ffd7; later `git pull` to 64689575 for BUILD_ONLY); the repo-root `gta_sa_compact.exe` of the main checkout was renamed to `.hidden` from before the configure until after the last clean-clone test (sha256 72ae59e4...cecc9b, 5,189,632 B before and after). `conan install` Release + Debug (shared cache, 6 s), then:

| step | command | result |
|---|---|---|
| configure | `cmake --preset StandaloneRelease --compile-no-warning-as-error` (no `-DGTASA_PYTHON`, no exe) | OK, `GTASA_ORACLE_EXE:FILEPATH=` empty, python found by itself |
| build | `build_locked.sh cmd /c start /low /b /wait ninja -C build/StandaloneRelease -j4 gta_reversed.exe` | 1184/1184, rc 0, 0 errors (clock 20:41:01 -> 20:42:28) |
| exe | `build/StandaloneRelease/bin/gta_reversed.exe` | 16,878,080 B, sha256 `986912f8ba6180b78e3a4c6bf88f2a38b66a11de97560c5acd5b978615855824`; ImageBase 0x400000, DllCharacteristics 0x8140 (ASLR on); 0 hits (ascii + utf-16) for `original_data`, `data_pointers`, `gta_sa_compact`, `gta_sa.exe`, `orig_image_pad`, `standalone_unknown_pointers`, `ORACLE` |
| run | `GAME_SRC=<Steam folder> OUT=C:/gta-scratch/g2-soak soak.sh build/StandaloneRelease 3 C:/gta-g2-run` (fresh data dir: hard links/junctions of the Steam folder + our exe), GPU lock | **RESULT OK**, started on attempt 1, route 393 s, 21 markers (menu, new game, walk, cars, rhino, idle, quit), errors 0, 10,200 frames, ~25 fps (windowed 960x600, mute), peak committed 564 MB, exit via atexit; `standalone.log`: `image base 0x00710000 (ASLR on, size 0x13EA000)` |
| tests | `BUILD_ONLY=1 run_all_tests.sh build/StandaloneRW` (fresh Debug configure, 42 test targets, ninja ok), then `SKIP_BUILD=1 run_all_tests.sh` under the GPU lock | **42 tests: 19 ok, 23 SKIP(no exe), 0 fail**, exit 0 |

No-original-exe evidence:
* the exe was absent from the clean-clone tree and from the main checkout root (`.hidden`); `RW_EXE_ORACLE` / `GTA_EXE` / anything `*ORACLE*` / `*ORIGINAL*` was not set in the environment; the clone's CMakeCache has no `gta_sa` path (only the empty `GTASA_ORACLE_EXE`);
* the run dir `C:/gta-g2-run` held (`g2-soak/datadir_inventory.txt`): `gta_reversed.exe` (the sha above), the Steam folder's own `gta-sa.exe` (v3.0, 5,971,456 B, sha256 15e3cfed...efb, a hard link of the Steam file, not the oracle), `eax.dll`, `ogg.dll`, `vorbis.dll`, `vorbisFile.dll`; no `gta_sa.exe`, `gta_sa_compact.exe`, `original_data.*`, `data_pointers.bin` (soak.sh checks and would have exited 2);
* the process module list during the run (`C:/gta-scratch/g2-modules.txt`) shows only `gta_reversed.exe` and system DLLs;
* a handle/file-open trace was not possible (no Process Monitor / handle.exe); the evidence is absence + a clean successful run + the binary holding none of the names;
* a disk-wide search (`C:/gta-scratch/g2-exe-search.txt`) found other copies of the original exe on the disk: `C:\gta-bv-orig\gta_sa.exe`, `C:\gta-perf\orig\gta_sa.exe`, `C:\gta-scratch\wtfix{,b}\gta_sa_compact.exe` (FIX-AUDIT worktrees), the main checkout copy, and many stale `original_data.* / data_pointers.bin` stashes (`C:\gta-scratch\*stash*`, old soak dirs, `build/*/bin` of the main checkout). None is referenced by the clean clone, its presets, caches or data dir, so the build/run could not reach any of them; they were left alone (other agents' dirs).

Non-oracle tests (19, clean clone, exe absent): rw_atomic_clump 274, rw_custom_pipelines 137, rw_engine 127, rw_frame_camera 118, rw_im2d_im3d 117, rw_matfx_pipeline 273, rw_matfx_uvanim 151, rw_math_stream 112, rw_pipeline 541, rw_platform 55, rw_raster_image 325, rw_renderstate 373, rw_rtanim_rtquat 4835, rw_skin_hanim 166, rw_skin_pipeline 52, rw_skin_vs 1 (its oracle sections are skipped without the exe: 1 check instead of 8,987), rw_strip 46, rw_texture 103, rw_world_geometry 166; 0 failures. SKIP(no_exe) (23): audio_fade, fixed, game, gameplay, logic, physics, pool, review oracle tests, rw_skin_cpu_oracle, rw_skin_hw_oracle, script_oracle, script_oracle_g13_15 ... g25_26 (11 script targets in total), world_oracle, x87_oracle.

## 3. Main checkout with the exe restored
`gta_sa_compact.exe` renamed back (sha256 72ae59e44c761389e354a50dc6215e964fe771121e2f4b1877273a493ceecc9b, 5,189,632 B; present in the repo root). `run_all_tests.sh build/StandaloneRW` (exe oracle ON, GPU lock): **42 tests: 42 ok, 0 SKIP**, exit 0 (logic_oracle 227 checks / 428 s, physics_oracle 107 / 291 s, rw_skin_vs 8,987 / 210 s, pool_oracle 1,900,009 checks). So the skips above are caused by the missing exe, not by a weakened test.

## 4. Notes
* `--compile-no-warning-as-error` is still needed on native MSVC 19.51 to configure the presets (documented in WINDOWS_SETUP.md); not changed here.
* Mac/Wine flows were not run.
* Scratch (disposable): `C:\gta-scratch\g2` (clone + builds), `C:\gta-g2-run`, `C:\gta-scratch\g2-soak`, `C:\gta-scratch\g2-*.log`, `g2-assets`.
