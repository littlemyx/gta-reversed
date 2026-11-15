# RYDER_1 input recording (SH2)  - committed as .notes/STATEHASH_INPUT.md

Files (this directory): `input.route.txt` = the complete input of the run (base route + recorded driver actions; what `replay` feeds to the exe as NOTSA_STANDALONE_INPUT), `record.driver` = only the recorded driver actions (228 lines), `record.route.txt` = the base route of the record run.
There is NO per-frame pad state. The input is a sparse event list on the injector's script clock (virtual ms, 20 ms per Idle frame). Text format, one item per line (a `;` also separates items), `type:arg`, ASCII, `\n` line ends, no binary fields; floats are `%.9g` of the float32 value (exact round trip).

## Items used (source: source/app/platform/SDLWrapper.cpp InjInit / InjPump / InjRunCmd, source/portable/mdriver.cpp)
| item | meaning |
|---|---|
| `wait:N` | script clock += N ms (relative, used by the boot part) |
| `at:T` | script clock = T ms (absolute, used by the recorded part; T is the script clock of the frame the action ran in) |
| `until:COND:MAXMS` | hold the script clock until COND is true (`menu`, `control`; the held real/virtual time is added to a shift so later `at:` times are in the shifted clock) |
| `mouse:x,y` / `key:NAME` | menu navigation: SDL mouse motion / key press (down, 150 ms later up) in the 960x600 window |
| `mark:NAME`, `label`, `skipif` | logging / flow, no game effect |
| `cmd:time:H:M` | set the game clock hour:minute |
| `cmd:rr_start` | start recording: `srand(12345)`, recorded frame counter = 0 |
| `mission:N` | free start of main.scm mission N (24 = GUNS1 = RYDER_1) |
| `cmd:mdkey:down|up:NAME` | the driver's key events: SDL key down / up, NAME = `return` (the enter-vehicle / pick-up / action key: SDLK_RETURN, CJ's triangle) or `w` (walk / drive forward); 48 events (13 return pairs + 11 w pairs) |
| `cmd:mdped:x:y:z:heading:cam` | teleport CJ (the web harness addToWorldAt): `CStreaming::LoadScene(p)` + `LoadSceneCollision(p)`, `CWorld::Remove(ped)`, `SetPosn`, `m_fCurrentRotation = m_fAimingRotation = heading`, `SetHeading(heading)`, `CWorld::Add(ped)`; when cam != 0 also `SetCameraDirectlyBehindForFollowPed_CamOnAString` + `RestoreWithJumpCut`; 25 events |
| `cmd:mdveh:x:y:z` | the vehicle CJ sits in (if any): `CCarCtrl::SetCoordsOfScriptCar(veh, x, y, z, 0, 1)` and move/turn speed zeroed (the same LoadScene calls first); 2 events |
| `cmd:quit` | end of the run (frame 25906) |
After each recorded action the driver writes a comment line `# rec=<recorded frame> gf=<CTimer frame counter>` (no colon: skipped by the injector): THAT is the frame mapping to use. Example: `at:151500 / cmd:mdkey:down:return / # rec=6622 gf=7589` = the key goes down during recorded frame 6622 (applied at the start of that frame's main-loop iteration, before its Idle / CGame::Process; so it is visible to frame 6622's simulation and hash).
Events are applied in the injector pump = once per main-loop iteration, BEFORE `Idle` of that frame, in file order; several events of one script ms run in the same pump. Order inside the pump: injector events, then the mission driver.
Key events reach the game as SDL key events -> the game's keyboard mapping (CPad: `w` = move forward, `return` = the action / enter-exit button); the held state of a key is what CPad reads in that frame's CPad::UpdatePads (first thing in CGame::Process).

## Where each class of action happened (recorded frames, from the `# rec=` comments)
First: `mdveh` at rec 4181, `mdped` 5142, ..., last `mdveh` ~25070 gf; `cmd:quit` rec 25906. The complete list with frames is `record.driver`; a one-line-per-action table: `py -I -c` over the file pairs `cmd:` / `# rec=`.
The driver steps themselves are the web port's `playRyder1` (tools/standalone/mdriver/ryder1.steps; trigger = the mission thread stood at one IP for 600 frames outside a cutscene). Not needed for replay: replaying `input.route.txt` gives the byte-identical hash file.
