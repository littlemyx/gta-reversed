# ASM-RVZ13: independent exe review of HC11c's 6 fixes

Re-disassembled every site (compare idiom, branch polarity for NaN/tie, operand order, constants 0.01/0.05/0.5/0.1/0.2/-0.1/250/800/100 read from the exe): all 6 HC11c commits CONFIRMED (4ce8bcbd6, 07c28cd9d, a81c04a5a, 73a236f12, b668d9acf, 5a84beffd), 16 sites. Rows: .notes/asm_sweep/RVZ13.result.tsv.

Extra (HC11c left it uncompared): CAutomobile::ProcessEntityCollision rhino extra-wheel mins (0x6AD24A-0x6AD33A, 8 floats). Exe: working copy [esp+0x38..] (collision-updated touch dists) vs saved door copy [esp+0x68..]: `fld touch; fcomp saved; test ah,5; jp` = `touch < saved ? touch : saved`. Port used std::min(touch, compr) = `compr < touch ? compr : touch`, which differs on tie / NaN / signed zero (kept touch, exe keeps the saved door value). Fixed with an explicit ternary. No effect for finite distinct values.

Side note: the exe ProcessEntityCollision depth term subtracts global 0xC1C214 (never written, 0), so it is an identity and the port correctly omits it.

Build: StandaloneRelease links (see commit). No game run (regress left to the gate).
