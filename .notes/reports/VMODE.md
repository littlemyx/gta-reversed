# VMODE - fresh-prefix `Failed: RwEngineSetVideoMode(-1)` (2026-10-16)

Root cause (NOT the missing gta_sa.set): the host reports 2 display adapters (built-in + external Dell in the Wine prefix registry), so
`MultipleSubSystems` is true. The original then shows the device/mode dialog (IDD_DIALOG1, 0x745E50). The standalone exe has no such
resource; `DialogBoxParam` returns -1 (truthy) -> `!CreateVidModeSelectDialog` false -> the video-mode block (`!MultipleSubSystems` only) is
skipped -> GcurSelVM stays -1. Original asm (0x746190..0x7463D4): with one sub system and display mode 0/invalid it loops GcurSelVM..N for the first
800x600x32 exclusive mode (only when IsVMNotSelected), else MessageBox "Cannot find 800x600x32 video mode" - our CheckDefaultVideoModeSupported matches that.
Proof: with a valid gta_sa.set (mode 1) it failed the same way; the new log line shows `MultipleSubSystems=1`.

Fix (WinPs.cpp, NOTSA_STANDALONE_RUN only): skip the dialog, keep sub system 0, run the normal mode selection for any sub-system count.
Also: NOTSA_STANDALONE_VIDEOMODE is now checked first in CheckDefaultVideoModeSupported (explicit override beats the auto search), index validated.
Diagnostic log lines added (`N sub systems: ...`, `UseDefaultVM=.. MultipleSubSystems=.. IsVMNotSelected=.. DisplayVideoMode=..`).

| Run (build/StandaloneRelease, 3 min, silent, private prefix) | Result |
|---|---|
| fresh prefix #1 (/tmp/vmode-prefix) | RESULT OK, 0 errors, mode 23 (960x600x32) |
| fresh prefix #2 (/tmp/vmode-prefix2, parallel) | RESULT OK |
| existing gta_sa.set (written by run #1, DisplayVideoMode=23) | RESULT OK, GcurSelVM=23 |
Before the fix: 4 fresh/with-set runs all FAIL at -1.

Docs: docs/STANDALONE.md (VIDEOMODE bullet + regression note), docs/WINDOWS_SETUP.md (first-session suspects corrected).
Note for Windows tonight: a PC with 2 monitors takes the same path (adapter 0 = primary).
Side facts: orchestrator's lock lives in the session scratchpad (`.../scratchpad/build.lock`), build_locked.sh default SCRATCH ($TMPDIR/gta-standalone-scratch) does NOT see it.
