# MAC-W0-REV-AB — wine MSVC A/B no-op proof (sub-task of MAC-W0-REV)

Repo /Users/Andrei.Mukhin/github/recomp/gta-reversed (main checkout, branch portable/main). DO NOT modify, commit or checkout anything in it (another agent edits there);
work only in /private/tmp/claude-501/w0_rev_ab (create it). Use `git archive` / `git -C <repo> show` read-only.

Goal: prove that HEAD=7203bc53 (B) is a no-op for the Windows MSVC build relative to A = `git rev-parse 3f46d831~1` (=8fcde553...), on 11 TUs, ignoring ONLY `__LINE__` values.
(A..B contains: W0/W0a/W0b commits, P-2a, P-2b (header changes: FileRecord.hpp, GenericGameStorage.h, RenderWare.h, MemoryMgr.h, ScriptParam.h, ...) and the removal of `#line N // W0a`.)

Prior art to REUSE (read, copy into your dir, adjust paths): /private/tmp/claude-501/w0_ab/{cfg.sh,b2.py,cmpobj.py,cmp2.py,jobs.txt} and report /Users/Andrei.Mukhin/github/recomp/gta-reversed/.notes/reports/MAC-W0b.md
(which describes the exact method: trees via git archive + rsync of untracked deps vendor/librw, source/libs/imgui, build/Release/generators, ConanPresets.json from the repo;
`cmake --preset StandaloneRelease -DGTASA_PYTHON=/opt/homebrew/bin/python3` under msvc-wine env as in cfg.sh; cl.exe under wine with PCH flags dropped and `/FI source/StdInc.cpp`;
same-path symlink swap `cur -> A|B` so absolute paths are equal; tools/standalone/obj_code_sig.py via cmpobj.py).

Steps
1. Trees: /private/tmp/claude-501/w0_rev_ab/{A,B}: A = git archive 3f46d831~1, B = git archive 7203bc53. Configure both (cfg.sh style). Compare compile_commands.json: same TU set; per-entry command identical after path normalisation (A vs B) and build.ninja identical modulo paths. Report counts. (Expect: identical. Anything different is a finding.)
2. Compile the 11 TUs (jobs.txt: AESmoothFadeThread, AEStreamThread, AEAudioHardware, AESound, AESoundManager, AEUserRadioTrackManager, FileMgr, WinPs, WinMain, WinPlatform, app_debug; variant `main`)
   for A and B to .obj with equal-length output dirs (sa / sb) and the same-path symlink. Max 2 parallel cl jobs (nice/taskpolicy as in b2.py). Wall-clock cap for you: 75 min.
3. Compare per TU: code + relocation signature (obj_code_sig, as cmpobj.py), --data, .debug$T, .debug$S sizes.
   Where code differs: this is EXPECTED to be possible ONLY through `__LINE__` immediates (assert/SPDLOG/ZoneScoped line numbers shift because guard lines were added). For every differing TU:
   disassemble both objs (llvm-objdump -d -r --no-show-raw-insn from `brew --prefix llvm`/bin, i386 COFF; or any available tool) and diff the instruction streams with addresses stripped;
   every differing instruction must differ only in an immediate operand, and that immediate must equal (a line in A's source) vs (the corresponding line in B's source) — verify by looking up the number in
   A's and B's source of the TU/headers (sed -n 'Np'). Classify each difference: `__LINE__ immediate (file:lineA->lineB)`, or `OTHER` (blocker: give exact instruction, function symbol, offset).
   Also compare the sets of relocation/symbol names (must be identical) and the .text/.rdata/.data section sizes (must be identical).
   If a diff is NOT a __LINE__ immediate, try to locate its source (which header/guard) with `cl /P` preprocessed output of A vs B (strip `#line` lines and blanks, diff) and report it.
4. Output: a table per TU: code bytes | code identical (strict) | differences classified (count of __LINE__ immediates, count of OTHER) | reloc identical | section sizes identical | .debug$T identical.
   Write it to /Users/Andrei.Mukhin/github/recomp/gta-reversed/.notes/reports/MAC-W0-REV-AB.md (git add -f is done by the parent; you just write the file) and return ≤ 12 lines to the parent (counts, any OTHER, path of the report).

Rules: no network use except none needed; no paid calls; do not touch the repo's working tree. Do not spawn sub-agents. Clean up wine processes you started (wineserver -k with the WINEPREFIX $HOME/.wine-msvc) when finished.
