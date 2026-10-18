# PT2-AA - playtest finding U17: anti-aliasing cannot be enabled (2026-10-18)

Fix commit on reverse/interior_c: f00acfab (cherry-picked from worktree branch pta2-u17, 00168685). Evidence (scratch): C:/gta-scratch/pta2_out/
(base_win = before, fix_win3 / fix_4k2 = after, orig = original exe dumps, soak, crop*.png = 4-6x crops, s3_hh_*.png = hour 11).

## Original exe (this PC, RTX 4080)
* Menu (asm 0x57D0C6..0x57D182, same as our CMenuManager code): option values 1..min(4, RwD3D9EngineGetMaxMultiSamplingLevels()) shown as Off / 1 / 2 / 3; accept -> prefs = value,
  RwD3D9ChangeMultiSamplingLevels(value), SetVideoMode(prefs mode), SaveSettings. gta_sa.set stores the raw value (int32 at file offset 0xB26 of the 2900-byte file).
* RW globals 0x8E242C max = 8 (levels 2..16 via CheckDeviceMultiSampleType for the mode format / !windowed, highest accepted; the NONMASKABLE quality count + 1 = 5 only raises it),
  0x8E2430 selected, 0x8E2434 = 5, 0x8E2438 nonmask. cdb dump of 0x8E2428 / 0xC9C040 (D3DPRESENT_PARAMETERS) with gta_sa.set AA pref 1/2/3/4 (exclusive 800x600, X8R8G8B8):
  | pref | selected | present MultiSampleType | quality | nonmask |
  |---|---|---|---|---|
  | 1 | 1 | 0 (none) | 0 | 1 |
  | 2 | 2 | 2 | 0 | 1 |
  | 3 | 2 (3 samples unsupported, stepped down) | 2 | 0 | 1 |
  | 4 | 4 | 4 | 0 | 2 |
  (SwapEffect forced to DISCARD whenever multisampled.)

## Repro / root cause
* Our shim (source/standalone/rw/engine.cpp) fed librw's `msLevel` straight into D3DPRESENT_PARAMETERS (startD3D / Reset). The menu offers levels 2, 3, 4; 3 samples does not exist on this GPU,
  so the Reset failed. Repro with the new injector `aa:N` (menu accept path), windowed 960x600, build before the fix: `aa:2` -> live type 2 ok; `aa:3` -> `dev=LOST`, the game segfaults a frame later.
  In the user's fullscreen run the option therefore "does not work" (picking the 2nd step kills the device); the other steps work only by luck of the driver. Also lost vs the exe: the quality level
  choice (q - 1), the early-out on the same level, the revert to the old parameters when the Reset fails, the max from the NONMASKABLE count.
* Not a stream-D / FIX-AUDIT regression: the shim has had this since P2B-02a (no bisect needed).

## Fix (f00acfab)
engine.cpp ports 0x7F6BF0 (level scan; at RwEngineOpen / RwEngineSetVideoMode), 0x7F84F0 (Set: clamp, step down, nonmask), the tail of 0x7F6CB0 (present parameters: highest quality level of the selected type,
else NONMASKABLE, DISCARD), 0x7F8A90 (Change: no-op on same level, rebuild + Reset, revert on failure); applied in ApplyModeToDevice (0x7F86B5) and RwEngineStart (one extra Reset when a level is selected before the start).
librw's msLevel is never set. Diagnostics: injector `aa:N`, `cmd:hour:H`; screenshot dumps resolve a multisampled back buffer (else the driver returns sample 0 = looks un-anti-aliased) and decode A2R10G10B10.

## Verification
* State parity with the original (same table): ours `[aa] 2 ... sel=2 nm=5 nonmask=1 live_type=2`, `3 -> sel=2 live_type=2` (no LOST), `4 -> sel=4 nonmask=2 live_type=4`, `8 -> live_type=8`, `1 -> none`; quality 0; max 8; nm 5.
* Image actually anti-aliased: windowed 960x600 and exclusive 3840x2160 (the user's A2R10G10B10 mode, NOTSA_STANDALONE_VIDEOMODE=51), CJ silhouette crops off vs 4x: stair-stepped vs smooth edges
  (crop3_aa1_b.png/crop3_aa4.png, crop4k2_aa1_b.png/crop4k2_aa4.png); 960x600 mid-gradient edge pixels in the CJ region 3562 (off) -> 4234 (4x), wall-top region 2824 -> 3178.
* Toggling off/2/3/4/8/off, a live video-mode change (vmode:15) with 4x on, and 4x again at 1600x900: device ok every time, no artefacts; hour 11 (heat haze) with 4x (s3_hh_aa4.png vs s3_hh_off.png,
  1600x900 variant): no squares / dark patches (PT-RENDER's front buffers survive the Reset).
* Gates: Release build (StandaloneRelease) ok; rw_* 21/21 ok (rw_engine_test 143 checks incl. start with 4x selected + requests 2/3/4/5/99/1/0 checking the live swap chain type; rw_platform_test
  Reset-count rows now toggle the level because the exe's same-level call is a no-op); soak 3 min windowed RESULT OK (0 errors, 0 game [error]; 2 gate timeouts = the known invehicle route flake).
* Not done: pixel A/B against the original in gameplay (the original was compared by its D3D present parameters / RW globals, which fully determine the device; driving it through intro + new game was not worth the lock time);
  the game-oracle tests were not rebuilt (no touched file is linked into them).
* Note: other agents' runs rewrite the user's Documents gta_sa.set; the user's version at 09:45 is saved as C:/gta-scratch/pta2_user_gta_sa.set.orig (AA pref 1 in it).
