# ASM-P4R10: signed vs unsigned time compares
Sites: ~45 hooked functions checked in exe (grep of source/game_sa minus Scripts, plus automated scan of every hooked function in Events, TaskTypes, Entity/Ped, Entity/Vehicle up to Vehicle.cpp::ProcessBoatControl for `now - x` followed by signed jcc).
Fix: CTaskComplexKillPedOnFootStealth::ControlSubTask (0x62B999) unsigned -> signed (e673b5bf9).
NOT_REACHED: scan of rest of Vehicle.cpp, Population, Garage*, PedGroup, Pickup, Gang*; part (2) int-vs-float storage.
Build: StandaloneRelease links (ninja -j4).
