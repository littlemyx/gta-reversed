# ASM-RVZ12: independent exe review of HC8e/HC11b's 5 min/max fixes
Helpers re-derived from the exe: 0x404330 min(a,b)=a<b?a:b, 0x420800 max(a,b)=a>b?a:b, a=[esp+4]=last pushed arg, NaN -> b.
All 5 commits (f5f1f10ee HydraulicControl, fb33fd71f NitrousControl, 997b2287e CBmx::ProcessAI, 8c9325d2c CTrain gas, ab9a1c287 DoVehicleLights zr350) CONFIRMED instruction-level: operand order, slot mapping, NaN/-0 results, constants (0.25, 0.001, 0.4, 2.0, 0.6981317, 0.01), z,y,x dot order, branch conditions. No source change, no build needed.
