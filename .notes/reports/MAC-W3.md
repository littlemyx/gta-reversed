# MAC-W3: real audio on the native Mac (W-3 hardware layer, W-4 decoders)

Branch portable/main, commits 29500d6a (+ one follow-up), pushed to fork. Own build dir build/PortableMacW3 (R5 uses build/PortableMac). Time ~45 min.

## Decision (hardware layer)
Upstream OpenAL skeleton (game_sa/Audio/OpenAL, 347 lines, OALBase/Buffer/Source, not wired) evaluated and NOT used: the game's hardware layer is DirectSound code that treats a secondary buffer as a RING that it keeps rewriting while it plays
(CAEStaticChannel::Service re-locks the loop region of a playing buffer, CAEStreamingChannel refills 0xC0000 bytes behind GetCurrentPosition) and sets volume/pan/frequency/3D at any time. One AL source per DS buffer would need the same
cursor/refill emulation plus a per-buffer queue pump. So: hardware layer unchanged; `source/portable/dsound_null.cpp` -> `dsound_al.cpp` (git mv): DirectSound semantics as a SOFTWARE MIXER (what DirectSound itself does) whose output goes through
ONE OpenAL streaming source (default device | ALC_SOFT_loopback | no OpenAL = steady clock). Files: dsound_al.{cpp,h}, winshim/dsound.h (factory name), audio_cmd.cpp, tests/portable/dsound_al_test.cpp, tools/portable/run_dsound_al_test.sh.

## Mapping
| DirectSound | here |
|---|---|
| buffer | ring bytes (Lock/Unlock real pointers, wrap split, FROMWRITECURSOR/ENTIREBUFFER), 8/16 bit mono/stereo |
| play/write cursor | mixer read position (frames*block); write cursor = play + 50 ms while playing; non-looping end stops and resets to 0; Stop keeps the cursor |
| SetVolume (1/100 dB) | gain 10^(v/2000), -10000 = silence, ramped linearly over the chunk (no zipper) |
| SetPan | opposite channel * 10^(-|pan|/2000) |
| SetFrequency | linear-interpolation resampler, step = freq/mixRate; 0 = original; 100..200000 else DSERR_INVALIDPARAM |
| 3D buffer / listener | deferred (DS3D_DEFERRED applied by CommitDeferredSettings) vs immediate; DS left-handed right = top x front; azimuth -> pan (far ear cos(|p| pi/2)), inverse-distance rolloff (game sets rolloff 0 => none), HEADRELATIVE/DISABLE modes; no doppler/cones |
| EAX / FX | QI IKsPropertySet = E_NOINTERFACE (logged once), SetFX(n>0) = DSERR_UNSUPPORTED (radio bass EQ off), caps report 0 hw buffers => game picks 48 software channels |
| output | 512-frame s16 chunks -> AL_FORMAT_STEREO_FLOAT32 buffers (exact, no dither), 4 queued (~43 ms), source AL_SOURCE_RELATIVE + AL_DIRECT_CHANNELS_SOFT, HRTF and output limiter off |
Gotchas found with the loopback device: openal-soft's output limiter has a 1 ms look-ahead (shifts the signal by 48 frames), 16-bit output is dithered (use float), and a voice that runs dry fades its held LAST sample out over ~64 frames (leaks into the first frames after a restart): loopback chunks carry a 128-frame zero tail, a streaming device never runs dry.
Knobs (env): NOTSA_AUDIO=device|loopback|null (default device, falls back to null when no device), NOTSA_AUDIO_WAV=file (everything output, 16-bit stereo WAV, header kept valid every 0.5 s), NOTSA_AUDIO_RATE, NOTSA_AUDIO_DUMP=dir (raw bytes of the first 32 played buffers). tools/portable/mac_boot_run.sh now defaults to NOTSA_AUDIO=loopback + <tag>.wav (agent runs are not audible).
Injector commands (portable-only, 4 lines in SDLWrapper.cpp InjRunCmd): `cmd:radio:ID`, `cmd:radiooff`, `cmd:audiostat` (portable/audio_cmd.cpp).

## Acceptance
| # | check | result |
|---|---|---|
| 1 | tests/portable/dsound_al_test.cpp via tools/portable/run_dsound_al_test.sh [mutate]: real dsound_al.cpp on an ALC_SOFT_loopback device (ASan+UBSan), 219 checks: mono/stereo/8-bit passthrough bit exact (+-1), silence after end, status/cursors, volume -100..-6000/-10000, pan +-10000, frequency (24k->48k interpolation, 1:1, 2x), loop, SetCurrentPosition, Stop/resume, Lock wrap, clip, Duplicate, 3D (azimuth, deferred vs immediate, DISABLE, rolloff, max distance), Null mixer == OpenAL loopback (<=1 LSB), threaded real-time loopback + WAV capture (level 10000*10^(-0.3) exact), default playback device opens (silent buffer) | RESULT DSOUND_AL OK; mutate: 15/15 mutants killed (volume, pan side, freq ratio, no loop, no clip, lock wrap, 3D right axis, deferred, write cursor, end cursor, SetCurrentPosition, buffer sum, rolloff, Stop keeps cursor, 3D pan) |
| 2 | boot to main menu with audio (build/PortableMacW3, mac_boot_run.sh): log `DirectSound over OpenAL: output=loopback ... 48000 Hz`, silence buffer + 3 frontend SFX started (2 one-shots 44.1 kHz, one 22 kHz loop), first non-silent output after ~50 ms, WAV non-silent every second (peak 16891). Device mode (`NOTSA_AUDIO=device`): `device=MacBook Pro Speakers`, 6 s run, clean device close, no underrun note | OK |
| 2b | radio (W-4): `cmd:radio:3` -> 786432-byte 32 kHz stereo streaming buffer playing, WAV peak ~16000, L-R rms 3700, ZCR 2-3 kHz (music, not noise); station switch 3->7, `radiooff` (silence), restart 12, 50 s soak: no crash, no error log | OK |
| 3 | Win32 untouched | only portable-only files (source/portable/**, tests, tools) + SDLWrapper.cpp: `w0a_guardlines.py check source/app/platform/SDLWrapper.cpp` = TEXT OK (Windows view text identical) |
ctest: `portable.dsound_al` (+ `.mutate`) added to tools/portable/portable_tests.cmake (R5 had no edit there). Needs Homebrew openal-soft.

## W-4 decoders
The encrypted OGG streams (AEStreamTransformer + CAEVorbisDecoder over libvorbisfile from conan) compile and run natively: radio verified above through CFileMgr/path layer, CAEDataStream, stream thread, CAEStreamingChannel. CAEWaveDecoder compiles, no stream in the Steam copy was exercised through it (all 1922 TRAKLKUP entries are in the encrypted packs; format sniffing happens after the transform) - NOT runtime verified. User tracks (MF/WMA, W-B5) stay logged-failing as agreed.

## Findings / not done
* Exit path crash (pre-existing, not audio: reproduced with NOTSA_AUDIO=null): after `cmd:quit` the normal shutdown ends in `Fixups::Log` called from a static destructor (std::mutex::try_lock throws) -> abort, exit code 139. Owner: standalone/app. The audio device closes cleanly before it.
* Menu loop at 22 kHz is a single 2 Hz sine cycle (game data); audible only through game-side frequency scaling.
* GetCurrentPosition is the MIXER position (up to ~43 ms ahead of what is audible); fine for the game's refill logic, not sample-accurate lip sync.
* Not audible-checked by ear (agent runs use loopback); device mode ran 6 s on the MacBook speakers with only the quiet menu sounds.
* BOOT1 note unchanged: SDL3 swap waits on CVDisplayLink when the display sleeps (not mine). path_resolve test still not in portable_tests.cmake (BOOT1 item).
