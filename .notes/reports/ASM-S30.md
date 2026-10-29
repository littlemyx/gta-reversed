# ASM-S30 (Camera.cpp, 35 functions)

Result table: .notes/asm_sweep/S30.result.tsv. 14 compared (12 MATCH, 2 FIXED), 2 PARTIAL, 19 NOT_REACHED (budget spent on S15e first).

- FIXED ResetDuckingSystem (b23658e0): the exe compares the SQUARED ped speed with 1e-6; the port used Magnitude(), picking the 0.3-1.0 / 0.3-0.5 duck factor differently for slow ducking peds.
- FIXED DrawBordersForWideScreen (9187c3e6): CRect literals rewritten with the {p0,p3,p2,p1} rule (the exe's raw rect stores have y1 = -5 / rect.bottom at +4 and y2 = rect.top / H+5 at +0xC; the port had them swapped; solid fill, so no visible change).
- MATCH: GetRoughDistanceToGround, ProcessShake, ProcessObbeCinemaCameraCar / Plane, ConsiderPedAsDucking, HandleCameraMotionForDucking (+DuringAim), CalculateFrustumPlanes (non-mirror branch line by line), CalculateDerivedValues, SetCameraUpForMirror, RestoreCameraAfterMirror, IsSphereVisible-3.
- UNSURE (not changed): shared StepCineyCamSeq. The exe stores a negative (dir+idx)%n remainder in the idx global (table lookup uses n-1); the port stores n-1. Only with stick direction -1; the exe then reads before the table start.
- PARTIAL: ProcessObbeCinemaCameraBoat (tail), IsItTimeForNewCamera (prologue only; the 30-case jump table 0x51E4E8 open).
- Open (NOT_REACHED): TryToStartNewCamMode, CamControl, Process, CameraColDetAndReact, UpdateTargetEntity, ImproveNearClip, CalculateGroundHeight, SetNearClipBasedOnPedCollision, UpdateSoundDistances, CameraGenericModeSpecialCases, Train / Heli cinema, ProcessJiggle and the setup / loader rows.
