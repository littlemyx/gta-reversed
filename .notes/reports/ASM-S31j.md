# ASM-S31j (Cam.cpp, 1 row)

Result table: .notes/asm_sweep/S31j.result.tsv. Process_Cam_TwoPlayer (exe 0x525E50-0x5267BF) compared in full: FIXED (1 commit, 4bebc479).

- |vel|^2 (ped0 + ped1 move speed) adds z, x, y in the exe (0x5260B0); the port added y, x, z (1-ulp float differences feeding the `<= 0.01` test and the sqrt).
- `pow(0.8f, timestep)`: the base is the float constant 0x8CC5E0 (0.800000011920929); the port used the literal 0.800000012.
- NaN polarities (the run build must follow the exe): angle wraps `a > h+pi` / `a < h-pi` (a NaN is left unchanged, the port subtracted 2 pi), first 0.1 clamp (NaN keeps its value), `mag2 <= 0.01` (NaN takes the sqrt arm), `d > pi` / `d < -pi`, `sqrt(mag2)*f3 > 1`, the f2 vs f5 clamp and the final 0.1 clamp.
- MATCH: pad switch and n2PlayerPedInFocus update, mode 0/1 follow-ped/follow-car dispatch (cam target set only where the exe sets it), the 21-step angle search (even/odd sign, int/2, lastClear/lastBlocked), both TryStep blocks (beta zeroing conditions), the aim pad term (-aim0 - aim1, 128 clamp, fov*0.0125*0.0714285746*|x|*0.007*0.007*x), beta and horizontal angle update, LOS args + help message, up vector, fov 70, m_bResetStatics.
- Not checked: IsSwitchPressed body (CPad 0x5404A0), ComputeTwoPlayerCamPos (0x5132D0), IsTwoPlayerCamPosClear (0x513220). The same `std::pow(0.800000012, ...)` literal exists at Camera.cpp near line 1351 and in Process_Cam_TwoPlayer_InCarAndShooting (other functions, not in this slice); worth a look.
