// MDRIVER-1: generic scripted mission driver for the deterministic RR build (-DGTASA_MDRIVER=ON, needs -DGTASA_STATEHASH=ON). Exists ONLY with the flag: nothing is compiled or hooked without it.
// NOTSA_MDRIVER=<steps file> (format: tools/standalone/mdriver/README.txt) makes the driver watch the mission thread and act through the SAME injector commands a replay route uses
// (`cmd:mdkey`, `cmd:mdped`, `cmd:mdveh`, `cmd:quit`); NOTSA_MDRIVER_OUT=<file> receives every action it performed as injector route lines (`at:<script ms>` + `cmd:...`), so the run replays WITHOUT the driver.
#pragma once
#ifdef NOTSA_MDRIVER
#include <cstdint>
#include <string>
namespace notsa::mdriver {
bool Enabled();
//! once per injector pump (= per frame), after the injector's due events; `clockMs` = the injector's script clock
void Tick(uint64_t clockMs);
//! injector command (`cmd:mdkey:down:return`, `cmd:mdped:x:y:z:h:cam`, `cmd:mdveh:x:y:z`); true when handled
bool Cmd(const char* name, const std::string& args);
//! SDL key push implemented by the injector (name: return, w)
void SetKeyPusher(void (*fn)(const char* name, bool down));
//! the injector's own command runner (for `quit`)
void SetCmdRunner(void (*fn)(const std::string& cmd));
}
#endif
