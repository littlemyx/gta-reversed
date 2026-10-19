# ORACLE-W3-PED state (for a continuation agent)

Landed: see `.notes/reports/ORACLE-W3-PED.md` (64 rows, 8 fix commits, Entity/Ped covered 30 -> 94, A/B uncovered 33/44 -> 3/10, whole game 2965 -> 3029).
Worktree recipe: `tools/standalone/windows/new_worktree.sh <id> --preset StandaloneRW`, then `cmake -S . -B build/StandaloneRW -DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON`.
Build/run: `ninja -j4 -C build/StandaloneRW wave3_ped_oracle_test` (via build_locked.sh, background), `RW_EXE_ORACLE=<gta_sa_compact.exe> wave3_ped_oracle_test.exe [row substrings] [-v] [W2_ASSERT_STACK=1]`. Resolve fault addresses with the .map (first column of the "deep stack").
Regenerate rows: `py -X utf8 -I tests/standalone/tools/gen_w3_ped.py C:/<wt> <sites.json> tests/standalone --classes A,B --skip tests/standalone/tools/w3_skip.tsv` (sites.json from `tools/standalone/oracle_coverage.py --json`). Exe asm: `tools/standalone/exedis.py --exe <exe> --repo . 0xADDR`.
Finding unresolved callees of a "NOT ORACLE-ABLE" row (every case faults at EIP 00400000): relink the test without /FORCE:UNRESOLVED (take `ninja -t commands`), read "unresolved external symbol ... referenced in function CPed::X", add the TU to PED_CALLEE_TUS in gen_w3_ped.py.

## Remaining (priority order)
1. Skipped rows with a concrete fix: wire `m_pedSpeech` (CAEPedSpeechAudioEntity at CPed+0x294: m_IsInitialized, m_PedAudioType, m_VoiceID in range) for EnablePedSpeech, EnablePedSpeechForScriptSpeech, CanPedHoldConversation; wire a real CWanted (zeroed with valid arrays) for SetWantedLevel / SetWantedLevelNoDrop / CheatWantedLevel; hand row for `operator delete(void*)` 0x5E4760 with a pool ped (rf::PedE(i)); hand row for `CanIKReachThisTarget` 0x609F80 with a CWeapon (a ped weapon slot).
2. Not constructible: SetupLighting / RemoveLighting (renderer), StopNonPartialAnims / RestartNonPartialAnims (clump anim list), SetPedAtomicVisibilityCB (RpAtomic).
3. Low-information rows: `CPlayerPed::IsHidden` (88/120 cases fault in both: CPhysical::GetLightingTotal needs lighting state), `ControlButtonSprint` (114/120: CPad state), `FlagToDestroyWhenNextProcessed`: improve the fixture (pad object, CPhysical lighting fields) to raise the share of non-faulting cases.
4. Hazard: rows whose argument is a pool ref / index (`operator new(size, ref)`, possibly `SpawnFlyingComponent(int32)`) must be range-limited; a random int made the pool write its flag byte at an arbitrary address, which corrupted the heap of the whole run (row names printed as garbage, exit 6 / 127 / 139). The harness ends this target with `_exit` as a belt-and-braces measure; check `SpawnFlyingComponent` first if a crash comes back.
5. Classes C / S / D of Entity/Ped (38 / 26 / 61) were out of scope.
