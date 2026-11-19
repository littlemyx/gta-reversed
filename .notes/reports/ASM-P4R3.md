# ASM-P4R3: story-path handlers in Scripts/Commands/Ported/Group*.cpp (CarCommands.cpp does not exist)
Review of P4R2n: CWeather::Update lightning (a24dd9d05) CONFIRMED; CShotInfo::Update sum orders (5df0c19ed) CONFIRMED.
Zone list: 485 TODO handlers (C:/gta-scratch/p4r/p4r3_list.tsv). Compared 26 hot-first (SWITCH, car/object damage and water, CLEAR_AREA, CREATE_OBJECT_NO_OFFSET, SLIDE_OBJECT, task handlers, heading/sqrt, coords). 0 fixes: int/float stores, 0.02f and deg-rad factors, ground-z, argument order all match the exe.
NOT_REACHED: the remaining ~460 handlers, mostly trivial getters/setters.
