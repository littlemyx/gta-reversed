# ASM-RVZ7: independent review of HC6 fixes
- CAudioZones::Update (0x5083C0): CONFIRMED. Skip test dist<20 strict (NaN continues), sphere sum z,y,x strict <, box int16*0.125 strict min<p<max, both lists capped at 10; port == c183db3bc.
- UpdateGasPedalAudio (0x4F5EB0): CONFIRMED. type 0 -> +0x964, type 9 -> +0x808 (both VALIDATE_OFFSET'd), others return; step_to == exe min/max incl. NaN/equal; constants 0.09/0.07 match.
- 8 highest-count SKIP-AUDIO rows (ConvertFromBytesToMS, AudioLog10, HasRadioRetuneJustStarted, GetCurrentTimeInMS, Get{Volume,Frequency}For{DummyIdle,DummyRev}): no rand call, no game-state write in the exe. Verdicts stand.
No source changes, no corrections needed.
