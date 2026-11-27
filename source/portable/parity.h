// PARITY (C:/gta-scratch/parity/FORMAT.md): frame-indexed input record / playback for the TS == C++ per-frame parity check. Exists ONLY with -DGTASA_STATEHASH=ON; every entry point
// is a no-op unless NOTSA_PARITY_REC=<file> (record) or NOTSA_PARITY_PLAY=<file> (play) is set, so a normal run is unchanged.
//   NOTSA_PARITY_REC=<file>   JSONL input file: one record per recorded frame (frame 0 = the `cmd:rr_start` frame) that has a stand command or a change of the pad / keyboard / mouse state
//   NOTSA_PARITY_PLAY=<file>  feed such a file back: stand commands run in the injector pump of their frame (before that frame's Idle / CGame::Process), pad / keyboard / mouse states
//                             overwrite what the platform layer sampled, at the exact places CPad reads them. Run it with the mission driver OFF.
//   NOTSA_PARITY_HASH=<file>  alias of NOTSA_SH_FILE (per-frame SH1 hash file, plus the extra `sc` column)
#pragma once
#ifdef NOTSA_STATEHASH
#include <functional>
#include <string>
class CPad;
namespace notsa::parity {
bool Recording();
bool Playing();
//! stand command that ran in the current recorded frame (not before `cmd:rr_start`); `mdkey` is not recorded (the key shows up in the keyboard / pad state)
void RecCmd(const std::string& cmd);
//! CPad hooks (no-ops when neither switch is set / not recording): pad = CPad::Update(0) after the keyboard+joy+mouse reconcile; mouse = UpdatePads after UpdateMouse; keys = UpdatePads after the key state exchange
void PadHook(CPad& p);
void MouseHook(CPad& p);
void KeysHook(CPad& p);
//! once per injector pump after the due injector events and before the mission driver: PLAY runs the commands of every frame <= the current one (run = the injector's command runner; false = not done yet, retried first in the next pump)
void PlayPump(const std::function<bool(const std::string&)>& run);
//! statehash OnFrame (recording on) with the frame number just hashed, before the counter advances: writes the record of that frame
void OnFrameEnd(unsigned frame);
}
#endif
