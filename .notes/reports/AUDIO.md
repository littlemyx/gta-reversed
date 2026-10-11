# AUDIO report: crackle hunt (Release standalone, silent runs)

## Verdict
Four port deviations found by diffing the audio hardware layer against the exe asm (all fixed, hooks unchanged). A silent DirectSound tap (`NOTSA_AUDIO_TAP`) shows NO stream-side problem
(0 underruns / stale halves / writes under the play cursor / seam jumps) even with frames up to 1.5 s long: the stream refill runs on its own thread (CAEStreamThread::MainLoop, ~105 ms period) and never
depends on the main loop. Static SFX are mixed/serviced on the main thread (see "Stutter relation").

## Port bugs fixed (exe address, what the exe does)
| # | where | bug | effect |
|---|---|---|---|
| 1 | CAESmoothFadeThread::Service 0x4EED10 | ramp was `start + LOG10_2*log2(step*progress)*20`; exe = `start + 20*log10(1 + step*progress)` (step = 10^(diff/20)-1) | fade-out: log of a negative -> NaN -> SetVolume(INT_MIN) (DSound rejects it, the volume sticks), fade-in: -inf/huge negative at the start. Every >60 dB step and every Stop() of an audible SFX goes through this. Oracle-proven (below) |
| 2 | CAEStaticChannel::SetAudioBuffer 0x4F0C40 (+PlaySound call) | `size` declared uint16; exe takes the full 32-bit byte count (`cmp edi,esi`, `mov [ebp+2Ch],edi`) | every sound > 65535 bytes (1.5 s at 22 kHz: loops, ambience, speech, engines) played as its `size % 65536` prefix, looped with a hard cut. Tap: 6-10 of ~2700-8000 PlaySound calls per soak route are > 64 KiB |
| 3 | CAEStreamingChannel::SetFrequencyScalingFactor 0x4F2060 | fade-in on un-pause was requested with bStopBufferAfterFade = true (exe pushes 0) | radio buffer was STOPPED at the end of every un-pause fade |
| 4 | CAEStreamingChannel::FillBuffer 0x4F1E20 | `m_nStreamPlayTimeMs` read vtable slot 2 (GetStreamLengthMs); exe calls slot 3 (`call [eax+0Ch]` = GetStreamPlayTimeMs, 0x502640) and null-checks the decoder | radio track position = track length |
Smaller fidelity fixes in the same files: CAEAudioChannel::SetVolume fade threshold is `abs((int)(v - cur)) > 60` (was fabs(float) > 60); SetFrequency / SetOriginalFrequency take a uint16 in the exe (`movzx`), the port now truncates the same way
(72000 Hz -> 6464); freq = (double)orig * factor then _ftol (product stays extended); CAEStaticChannel::Service compares `(uint32)(timer - sync) > (uint32)field_74` (exe `jbe`, unsigned), CAEStaticChannel::Play only SETS m_bPaused.
Checked equal to the exe (no change): CAEStreamingChannel::Service/PrepareStream/UpdatePlayTime, CAEStreamThread::MainLoop (Service + channel Service, sleep 5-elapsed then 100 ms, same as exe), RequestFade, SetBufferVolume, CancelFade, SmoothFadeProc, SetAudioBuffer body, StaticChannel::SynchPlayback,
GetSoundBuffer size/rate math, StaticInitialise (the exe also only ever writes table[1][1]).

## Tap (source/game_sa/Audio/Hardware/AEAudioTap.h, run build only, env NOTSA_AUDIO_TAP=<file>, documented in docs/STANDALONE.md)
Per buffer: Lock/Unlock (offset, size, play+write cursor at Lock and Unlock, FNV hash, first/last frame), GetCurrentPosition slot tracking, Play/Stop/SetCurrentPosition, SetVolume (thread, value, fade-entry range check), SetFrequency, SetPan, Release;
main-loop frame time (CAEAudioHardware::Service), stream-thread iteration time, fade-thread counters. Anomaly kinds: STALE_SLOT (underrun), MISSED_SLOT, WRITE_IN_PLAY, LATE_REFILL, SEAM_JUMP, CLICK, LOOP_WRAP, FADE_RANGE, FADE_REVERSE,
SETVOL_REJECTED, FREQ_REJECTED, STREAM_STALL; every line carries the time since the last >50 ms frame. Chained on top of the mute vtable.

## Verification
| check | result |
|---|---|
| soak.sh 10 min (muted, tap on; /tmp/audio-out/run3) | RESULT OK; 0 anomalies of any kind; 8093 PlaySound, 293k main-thread SetVolume, 97k SetFrequency, 0 rejected; frames up to 1076 ms (22241 frames, 3815 > 50 ms) |
| soak 8 + 7 min with radio on (run1/run2/run4) | 23-39 stream refills each: refill lead 2.87-3.0 s (half = 3 s at this track's rate), 0 STALE/MISSED/WRITE_IN_PLAY/LATE/SEAM; stream thread max iteration work 58-141 ms, max gap 174-248 ms; main frames up to 1.56 s in the same runs |
| WINEDEBUG warn+dsound,mmdevapi,winmm (run4) | no dsound/mmdevapi warning lines (no Wine-side underrun reports) |
| audio_fade_oracle_test (new, tests/standalone, Debug) | exe Service 0x4EED10 vs port on 3000 random entry tables x PC53/PC24 with fake IDirectSoundBuffers (clock patched): 70080 ramp steps, 43033 fade ends, 39138 cancelled entries, 96918 SetVolume calls: 0 mismatches at PC=53 (fade thread precision). At PC=24 499 cases differ only in the last bit of the stored float m_fCurVolume (SetVolume sequence identical) - informational |
| mutation: old `log2(step*progress)` formula | 2905 mismatches (e.g. exe SetVolume(-1909), old port SetVolume(-2147483648)) |
| StandaloneRelease gta_reversed.exe | builds, links (330/330) |
Not exercised in the soak routes: the fade RAMP itself (2894 RequestFade calls in run3: 2805 already at the target volume, 89 not playing, 0 ramps) - the route has few audible Stop()s; the oracle covers the code, not the route.

## Stutter relation (answer to the coordinator)
* Stream (radio) refill: separate thread, independent of the game thread. Measured: with 1.5 s main-loop frames the stream thread kept its ~105 ms cadence, 0 underruns.
* Static SFX: Lock/Unlock only at SetAudioBuffer (main thread) and, for sounds with a loop start, one rewrite in CAEStaticChannel::Service (main thread, after `field_74` ms of GAME time). The buffer is pre-filled with the loop body for >= 24000 bytes, so a main-thread stall of
  more than roughly 0.5 s after a loop start replays the intro snippet instead of the loop body (exe-inherent). 16-34 "rewrite under the cursor" events per soak (also exe-inherent: pitch < 1 stretches the intro beyond field_74).
* Volume/pitch updates (RescaleChannelVolumes, SetVolume steps of up to 60 dB without a ramp, SetFrequency per frame) are per-frame on the main thread: with host-saturated 15 fps they become coarse (zipper noise) - exe behaviour, not fixable here.
* Not visible from the game's DirectSound calls: Wine's own dsound mixer/CoreAudio underruns under host saturation (no dsound warnings were logged in run4, but the mixer thread competes with wined3d for CPU).

## Open
* The user's periodic crackle is not reproduced by any tap anomaly in the stream path; the fixed bugs (#1 fades, #2 truncated long sounds) are the plausible port-side causes (speech lines / long loops, fade-outs); needs the user's ear on a build without the mute.
* Soak routes should exercise speech and sound stops (honk, door, weapon cut-off) to cover ramps.

Commit: see git log (files: AEAudioTap.h, AESmoothFadeThread.cpp, AEAudioChannel.cpp, AEAudioHardware.cpp, AEStaticChannel.cpp/.h, AEStreamingChannel.cpp, AEStreamThread.cpp, tests/standalone/audio_fade_oracle_test.cpp, source/CMakeLists.txt, docs/STANDALONE.md).
