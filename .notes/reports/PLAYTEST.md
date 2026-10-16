# PLAYTEST report (2026-10-11)

* Build: build/StandaloneRelease, `ninja -j6 gta_reversed.exe` under the build mutex: 25 steps, 0 errors, 536 s of compile+link (04:08-04:17), after ~50 min waiting for the mutex (held by a StandaloneDetached full build). PCH not deleted: `ninja -n` showed no PCH-included header newer than the PCH (only 22 objects stale), a delete would have forced a full rebuild.
* Source state: HEAD at build start 170085d9 (includes shadow flicker f9b8b892 and audio fixes 170085d9). The tree also had       47 dirty files of other agents (list in the soak of that moment: codemod/Pool.h/Entity.h/Replay.cpp/Shadows.cpp/... WIP); they compiled and are IN this exe. A clean-HEAD exe would need a worktree full build (~1.5 h under current load, not done).
* Play dir: /Users/Andrei.Mukhin/gta-play (symlinks to the Steam install for anim audio data models movies stream.ini dlls..., copies of gta_reversed.exe 32.7 MB, original_data.bin/json, data_pointers.bin). gta_sa.exe is not linked.
* Prefix: ~/.wine-gta-play (cp -cR clone of ~/.wine-msvc, ShowCrashDialog=0), created once by play.sh if missing.
* play.sh: Wine Staging from ~/tools, WINEDEBUG=-all, unsets all NOTSA_* test env (no mute/inject/sampler/screenshot/skip videos), retries up to 8 times on the wined3d start flake, prints log paths (standalone.log, logs/log.log, wine.out in the play dir). Window mode is the exe's own (windowed 960x600 first mode >= 800 wide).
* Smoke test (muted, SKIP_VIDEOS): reached the Main Menu (START GAME / OPTIONS / QUIT GAME) in the play dir with the play prefix; test processes killed, test logs removed.
* Notes: without SKIP_VIDEOS the intro logo waits for a key (no DirectShow): press Enter/Space. Not verified by me: actual sound output (smoke test was muted).
