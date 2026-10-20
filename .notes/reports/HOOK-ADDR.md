# HOOK-ADDR report

Agent HOOK-ADDR (rules v3), 2026-10-20 14:41 - 15:40 WEST, worktree C:/gta-wt/hook-addr (base 09edca8c). Script: `tools/standalone/hook_addr_audit.py`
(`python -I tools/standalone/hook_addr_audit.py --exe gta_sa_compact.exe [--tsv out.tsv]`, ~15 s; sites from `oracle_coverage.inventory()`, capstone, raw xref scans).

Checks per site: address in .text; unique per port function; first instruction not ret/int3/nop/invalid (a `ret`/`ret N` is accepted as an empty function only when a
function boundary is in front: ret/ret N/int3/nop/jmp/switch jump table); address is a boundary or has a call/data/push-imm reference (E8 targets, dwords in .rdata/.data, `push`/`mov` immediates);
no other site lies inside the function body (linear walk + forward branch targets). The VIS case (0x732307: `add esp,0Ch` then bare `ret`) is flagged FIRSTRET by it.

Result: **9249 sites, 0 out of .text, 0 duplicate addresses, 91 empty-function stubs (ret after a boundary, accepted), 10 flagged sites -> 5 FIXED, 1 hook removed, 3 EXPLAINED, 0 unexplained.**

| site | address | verdict |
|---|---|---|
| ::ValidateVersion (Game.cpp) | 0x5BA0BC -> **0x5BA060** | FIXED. 0x5BA0BC is the `jl` target / loop inside the function; entry `sub esp,0x80` (opens models\coll\peds.col) is 0x5BA060 |
| ::IsActive (LoadingScreen.cpp/.h) | 0x744DB5 -> **0x744DB0** | FIXED. 0x744DB5 is the `ret` of `mov al,[0xBAB318]; ret` at 0x744DB0 (same bug class as VIS) |
| CVehicleModelInfo::SetRenderPipelinesCB | 0x4C88F4 -> **0x4C88F0** | FIXED. 0x4C88F4 is the `ret` of `mov eax,[esp+4]; ret`; oracle row moved to 0x4C88F0 and now compares the returned atomic exactly (was "!= 0" because of the bare ret): wave3_models_oracle_test 95 rows, 0 mismatches. Pre-fix the exact compare would read the caller's leftover eax |
| CTaskSimpleGiveCPR::FinishGiveCPRAnimCB | 0x658910 -> **0x658990** | FIXED. 0x658910 is mid-instruction inside ReviveDeadPed (0x658900, SEH prolog `mov fs:[0],esp` = 0x65890E..0x658914); the finish callback pushed at 0x659E50/0x65A810 is 0x658990 (`or byte [arg+8],2; mov [arg+0x10],0`) = port body |
| CConversations::EnableConversation | 0x43A7F0 -> **hook removed** | FIXED (removed). 0x43A7F0 is the middle of `add eax,0x18` inside Clear (0x43A7B0..0x43A804); no call/jmp/data ref to it, AwkwardSay follows at 0x43A810: the exe has no separate function (the script handler inlines it). In the DLL build the hook would have corrupted Clear's loop. Commented `// RH_ScopedInstall(EnableConversation, 0x0)` (repo convention) |
| CConversations::Clear | 0x43A7B0 | OK (INSIDE flag was the EnableConversation site above) |
| CTaskSimpleGiveCPR::ReviveDeadPed | 0x658900 | OK (INSIDE flag was the FinishGiveCPRAnimCB site above) |
| Win::NOTSA_WinMain | 0x748710 | EXPLAINED. Contains the Input::Initialise site 0x7487CF; WinMain is replaced as a whole (Locked), the inner site is a code fragment (see next) |
| Input::Initialise (WinInput.cpp) | 0x7487CF | EXPLAINED. Deliberately inside WinMain (inlined input init block, `call 0x530500` = ControlsManager.MakeControllerActionsBlank), not a function entry |
| notsa::WindowedMode::HookDirect3DDeviceReplacerSA | 0x7F6781 | EXPLAINED. notsa mid-function patch point in the RW D3D9 device creation (`call [edx+0x40]`), not a port of an exe function |

Previously flagged by the first script version and cleared by the jump-table rule (function entries that directly follow a switch table, real entries): 0x469EB0 CRunningScript::ProcessOneCommand,
0x4D6D40 CAEAmbienceTrackManager::IsAmbienceRadioActive, 0x4DBF00 CAECutsceneTrackManager::PauseTrack, 0x5BE670 LoadWeaponData, 0x52DD80 ControllerConfigManager::AffectControllerStateOn_ButtonUp_DebugStuff (`ret 8` stub).
Not covered by the script: a wrong but valid function entry (address of another real function); that needs names/xrefs, not asm shape.

## Gate
- StandaloneRelease `gta_reversed.exe`: built (1184/1184, no errors) in the worktree. The DLL/Debug (real hook patching) preset does not exist as a standalone preset here, not built; the changes are address literals only.
- `run_all_tests.sh build/StandaloneRW wave3_models_oracle_test`: 95 checks, 0 failures, 0 mismatches (the SetRenderPipelinesCB 0x4C88F0 row 20/20).
- `oracle_coverage.py`: covered **4525 -> 4525** (SetRenderPipelinesCB's row moved with its address and stays covered; the other fixed sites had no row), ported functions 9249 -> 9248 (EnableConversation hook removed; .notes/ORACLE_COVERAGE.md regenerated).
- The full run_all_tests is left to the orchestrator.
