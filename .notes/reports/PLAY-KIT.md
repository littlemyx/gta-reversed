# PLAY-KIT: play kit for the user (Windows host)

* Build: `build/StandaloneRelease` rebuilt at HEAD 917b3c52 (working tree incl. others' uncommitted edits, 1121 steps, 0 failures, `ninja -j4` via build_locked.sh).
* Play dir `C:\gta-play\`: junctions/hard links to the Steam folder (as soak.sh; Steam folder untouched, `gta-sa.exe` of Steam is only linked, never run) + SNAPSHOT copies of gta_reversed.exe, original_data.bin/.json, data_pointers.bin (a later rebuild does not change it), play.cmd, README-ru.txt.
* Launcher `tools/standalone/windows/play.cmd` (committed; copy in C:\gta-play), desktop shortcut "GTA SA (standalone)" -> `C:\gta-play\play.cmd`.
  `play.cmd` = fullscreen, sound ON; `play.cmd windowed` = 1280x720 window; `play.cmd videos` = intro videos.
  GPU lock `C:\gta-scratch\gpu.lock` (owner `USER-PLAY`, waits with "агенты сейчас запускают игру, жду…", released after the game exits/crashes);
  all test-only NOTSA_* variables are cleared; non-zero exit -> `crash-<stamp>.log` copy of standalone.log; logs in C:\gta-play.
  Test-only pass-through env: PLAY_MUTE=1, PLAY_NOPAUSE=1, PLAY_WINDOW=WxH, PLAY_VIDEOS=1.
* Test (silent, windowed 960x600): lock waited for D-B18's census, then acquired/released; main menu reached (back-buffer screenshot), `[mute]` active, WM_CLOSE quit -> exit code 0, no crash log, lock released; forced kill -> rc 1 + crash-*.log copy works. Game not left running.
* FINDING: with videos ON the native DirectShow intro hung on a black frame (no log activity, 1 core busy, > 3 min), so play.cmd SKIPS videos by default (`videos` opt-in). Videos were never validated natively (docs/WINDOWS_SETUP.md §7).
* Other notes: at the moment there is NO gta_sa.set in Documents (soak.sh deletes it; backup `gta_sa.set.soak-bak` is there, never restored automatically); the game creates a fresh one on the first quit - the previous saved settings file crashed starts (BUG-QUIT fixed the likely cause, librw f358ffd). If the first real start with a saved gta_sa.set crashes, delete it. Closing the console window with X kills the game and leaves the lock held (`rd /s /q C:\gta-scratch\gpu.lock`).
* Sound was NOT tested by ear; the mute test used the same DirectSound mute path as the soaks.
