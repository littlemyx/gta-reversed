# MUTE report (NOTSA_STANDALONE_MUTE)

Commit: 4e2fb21d (AEDirectSoundMute.h new, AEStaticChannel.cpp / AEStreamingChannel.cpp 1 line each, soak.sh, census.py, standalone_deviations.tsv, docs/STANDALONE.md, docs/STANDALONE_FIDELITY.md).

## Design
* Run build only (`NOTSA_STANDALONE_RUN`), env `NOTSA_STANDALONE_MUTE` (set and not "0"). Header-only `source/game_sa/Audio/Hardware/AEDirectSoundMute.h`.
* Clamping the `SetVolume` call sites was rejected: `CAESmoothFadeThread::RequestFade/Service` branch on `IDirectSoundBuffer::GetVolume`, so it would change fade logic.
  Instead each secondary buffer created by CAEStaticChannel::SetAudioBuffer / CAEStreamingChannel::Initialise is attached to a COPY of its own vtable with only
  `SetVolume` (slot 15) and `GetVolume` (slot 6) replaced: SetVolume validates the range like dsound, stores the logical volume (mutex-guarded map) and sets
  DSBVOLUME_MIN on the real buffer; GetVolume returns the logical one. Play/Stop/Lock/3D/status/fades/streaming run unchanged. The menu sliders cannot undo it.
* Silence buffer (CAEStreamingChannel::InitialiseSilence) is zero data without CTRLVOLUME: not attached, inaudible by construction.
* Log lines in standalone.log: `[mute] ...` first attach, swallowed-call counters (1/1000/100000/1000000), +5 dB steps of the loudest requested volume.
* standalone_deviations.tsv is a data-global table parsed by verify_globals.py, so the entry is a `#` comment line there; the real row is in docs/STANDALONE_FIDELITY.md (Platform).

## Wine audio driver disabled: REJECTED
`wine reg add 'HKCU\Software\Wine\Drivers' /v Audio /t REG_SZ /d '' /f` in a private prefix: the exe's `DirectSoundCreate` fails (mmdevapi "User explicitly chose no driver",
GetDefaultAudioEndpoint 80070490) and the game hangs at 0% CPU right after `CPU vendor:` in logs/log.log, never reaching the menu (6/6 soak attempts, manual runs). So only the env var is used.
soak.sh/census.py do NOT touch the registry. PORT_BRIEF.md "must be SILENT" section corrected accordingly.

## Verification (exe of 22:58, Wine Staging 11.18, private prefix /tmp/mute-prefix, no audio-driver change, muted from the first launch)
| check | result |
|---|---|
| StandaloneRelease objects | AEStaticChannel/AEStreamingChannel compile (incl. final header); full link blocked at the end by OTHER agents' dirty files (DecisionMakerTypes.cpp:44 'instance', Clouds.cpp:873 'randTable', ClothesBuilder.cpp:73 'defaultTxdIdx'), so the soaks used the 22:58 exe which differs from the commit only in the loudest-volume log condition |
| soak.sh 3 min (soak1, soak3, soak4) | RESULT OK x3, 0 errors |
| soak with WINEDEBUG=err+all,+dsound (soak2, 3 min) | RESULT OK; 161074 real `IDirectSoundBufferImpl_SetVolume` calls, ALL = -10000; 293 buffers played, 292 had SetVolume(-10000) before their first Play, the 293rd is the zero-data silence buffer |
| game audio code ran | 14 SFX banks opened, CAEPedSpeechAudioEntity active, 100000+ SetVolume calls through the hook |
Observation: in these runs the game never requested a volume louder than -30 dB (counter "0 of them louder than -30 dB"; settings are fresh defaults), so even an unmuted run would have been quiet; the mute is verified at the Wine call level (all -10000), not by the loudness of the game's own request.
Not verified: the +5 dB loudest-volume log lines (final header not linked into an exe, see above).
