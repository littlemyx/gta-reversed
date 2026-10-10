# S6-D table, part 1 (g16, 48 commands) - file source/game_sa/Scripts/Commands/Ported/Group16_17a.cpp
(part 2, g17, is in `.notes/S6D_TABLE_g17.md` / Group16_17b.cpp [+c])

Group processor: ProcessCommands1600To1699 @0x493FE0 (switch base 1602, 98-entry dword jump table @0x495E00). Every case ends `xor al, al` (OR_CONTINUE).
Columns: id | name | exe case address | params in (label = ReadTextLabelFromScript string, h = pool handle, f float, i int) | out | notes.
`GPST(h,task)` = CRunningScript::GivePedScriptedTask(handle, task, cmd) (0x465C20; handle -1 = task added to the open task sequence).

| id | name | case | in | out | notes |
|---|---|---|---|---|---|
| 1606 | GET_SEQUENCE_PROGRESS | 0x4940F3 | h | 1 i | CPedScriptedTaskRecord::GetStatus(ped, 0x618) == 0 (EVENT_ASSOCIATED; NONE=-1 continues!) -> -1; else ((CTaskComplexUseSequence*)primary[TASK_PRIMARY_PRIMARY])->m_nCurrentTaskIndex (no type check) |
| 1607 | CLEAR_LOOK_AT | 0x49415B | h | - | h == -1: new CTaskSimpleClearLookAt -> active sequence; else if g_ikChainMan.IsLooking(ped): AbortLookAt(ped, 500) |
| 1608 | SET_FOLLOW_NODE_THRESHOLD_DISTANCE | 0x4941F5 | h, f | - | intelligence+0xCC = p1 (raw dword) |
| 1611 | CREATE_FX_SYSTEM | 0x494228 | label(32), f x,y,z, i flag | 1 i | label read first; dead GetActualScriptThingIndex(peek dst var); z<=-100 -> ground; FxManager::CreateFxSystem(name,&pos,null,flag!=0); ok: AddScriptEffectSystem -> id, cleanup AddEntityToList(id,4); fail: -1 |
| 1612 | PLAY_FX_SYSTEM | 0x494323 | i fx | - | idx=GetActualScriptThingIndex(fx,1); idx>=0 && ScriptEffectSystemArray[idx].m_pFxSystem -> Play() |
| 1614 | STOP_FX_SYSTEM | 0x494361 | i fx | - | same, Stop() |
| 1615 | PLAY_AND_KILL_FX_SYSTEM | 0x49439F | i fx | - | PlayAndKill(); RemoveScriptEffectSystem(rawId); cleanup RemoveEntityFromList(rawId,4) |
| 1616 | KILL_FX_SYSTEM | 0x494402 | i fx | - | Kill(), then as 1615 (shared tail 0x4943D7) |
| 1620 | SET_OBJECT_RENDER_SCORCHED | 0x4944A1 | h obj, i on | - | physicalFlags (+0x40) bit 0x20000000 set/cleared, no null check |
| 1621 | TASK_LOOK_AT_OBJECT | 0x4944E9 | h ped, h obj, i time | - | time -1 -> 20000, -2 -> 0x7FFFFFFF; ped != -1: g_ikChainMan.LookAt("COMMAND_TASK_LOOK_AT_OBJ...", ped, obj, time, -1, null, false, .25, 500, 6, true); -1: new CTaskSimpleTriggerLookAt(obj,time,-1,{0,0,0},true,.25,1000,3) -> sequence |
| 1622 | LIMIT_ANGLE | 0x4945D5 | f | 1 f | while <0 +=360; while >360 -=360 (x87 NaN/PF semantics kept) |
| 1623 | OPEN_CAR_DOOR | 0x49467B | h car, i door | - | node=GetCarNodeIndexFromDoor(door); return if vtbl[38] IsDoorMissing(door); return if CAutomobile::m_aCarNodes[node] (+0x648) null; vtbl[27] OpenDoor(null,node,door,1.0,true) |
| 1627 | GET_PICKUP_COORDINATES | 0x4946E4 | i pickup | 3 f | idx=GetActualPickupIndex; -1 -> (0,0,-100) else aPickUps[idx].GetPosn() |
| 1628 | REMOVE_DECISION_MAKER | 0x494767 | i id | - | idx=GetActualScriptThingIndex(id,7); 0<=idx<20 && RemoveFromResourceManager(id,3,script) -> UnloadDecisionMaker(idx); cleanup RemoveEntityFromList(id,9) |
| 1633 | BREAKPOINT | 0x4947EC | label(40) | - | NOP (reads the label only) |
| 1639 | TASK_AIM_GUN_AT_COORD | 0x494868 | h, f x,y,z, i time | - | GPST(h, new CTaskSimpleGunControl(null,&pos,null,0,500,time)) |
| 1640 | TASK_SHOOT_AT_COORD | 0x4948EF | h, f x,y,z, i time | - | h != -1 && weaponinfo(activeWeaponType,1)->flags bit8 (bThrow): GPST(h, CTaskSimpleThrowControl(null,&pos)); else GPST(h, CTaskSimpleGunControl(null,&pos,null,3,5,time)) |
| 1641 | CREATE_FX_SYSTEM_ON_CHAR | 0x4949F9 | label(32), h ped, f x,y,z, i flag | 1 i | peek dead call; z<=-100 -> ground(x,y at the OFFSET); CreateRwObject if none; GetModellingMatrix null -> -1; CreateFxSystem(name,&off,mat,flag!=0) |
| 1642 | CREATE_FX_SYSTEM_ON_CHAR_WITH_DIRECTION | 0x494A6E | label, h, f x,y,z, f dx,dy,dz, i flag | 1 i | transform via g_fx.CreateMatFromVec(&mat,&off,&dir) (0x49E950); CreateFxSystem(name,mat,modelMat,flag) (0x4A9BB0) |
| 1643 | CREATE_FX_SYSTEM_ON_CAR | 0x494B70 | as 1641 (vehicle pool) | 1 i | |
| 1644 | CREATE_FX_SYSTEM_ON_CAR_WITH_DIRECTION | 0x494BAC | as 1642 (vehicle pool) | 1 i | |
| 1645 | CREATE_FX_SYSTEM_ON_OBJECT | 0x494BEC | as 1641 (object pool) | 1 i | |
| 1646 | CREATE_FX_SYSTEM_ON_OBJECT_WITH_DIRECTION | 0x494CB8 | as 1642 (object pool) | 1 i | |
| 1650 | TASK_DESTROY_CAR | 0x494DC6 | h ped, h car | - | GPST(h, CTaskComplexDestroyCar(car,0,0,0)); car may be null |
| 1651 | TASK_DIVE_AND_GET_UP | 0x494E34 | h, f x, f y, i time | - | dir = Normalise(x,y,0) (0x59C910 semantics); GPST(h, CTaskComplexEvasiveDiveAndGetUp(null,time,dir,true)) |
| 1652 | CUSTOM_PLATE_FOR_NEXT_CAR | 0x494EA4 | i model, label(9) | - | label AFTER the model; '_' or NUL of the first 8 chars -> ' '; mi=ms_modelInfoPtrs[model]; mi && type==6 && rwObject -> SetCustomCarPlateText |
| 1654 | TASK_SHUFFLE_TO_NEXT_CAR_SEAT | 0x494F1D | h, h car | - | GPST(h, CTaskComplexShuffleSeats(car)) |
| 1655 | TASK_CHAT_WITH_CHAR | 0x494F85 | h, h partner, i lead, i b4 | - | GPST(h, CTaskComplexPartnerChat("COMMAND_TASK_CHAT_WITH_CHAR", partner, lead!=0, .5, b4!=0 ? -1 : 4, true, true, {0,0,0})) |
| 1663 | FORCE_CAR_LIGHTS | 0x49539D | h car, i mode | - | byte +0x4A8 bits 3..4 (m_nOverrideLights) = mode |
| 1664 | ADD_PEDTYPE_AS_ATTRACTOR_USER | 0x4953DD | i effect, i pedType | - | idx=GetActualScriptThingIndex(effect,6), 0<=idx<64: ms_userLists[idx] (0xC3A200, stride 0x24): bUseList=1, first slot ==-1 -> -2 + pedType (inlined 0x492CB0) |
| 1665 | ATTACH_OBJECT_TO_CAR | 0x495425 | h obj, h car, f x,y,z, f rx,ry,rz | - | obj->AttachEntityToEntity(car, off, rot*0.017453292) (0x54D570), no null checks |
| 1666 | DETACH_OBJECT | 0x4954E2 | h obj, f x,y,z, i flag | - | obj && obj+0xFC(m_pAttachedTo): DettachEntityFromEntity(x*K, (y*-1)*K, z, flag!=0) (K=0.017453292) |
| 1667 | ATTACH_CAR_TO_CAR | 0x49557B | h, h, f x,y,z, f rx,ry,rz | - | x <= -999.9 (ordered): AttachEntityToEntity(car2, null, null) (quat overload 0x54D690); else vec overload with rot*K |
| 1668 | DETACH_CAR | 0x495657 | h, f x,y,z, i flag | - | as 1666 |
| 1669 | IS_OBJECT_ATTACHED | 0x4956D3 | h | cmp | obj && +0xFC != 0 |
| 1670 | IS_VEHICLE_ATTACHED | 0x495719 | h | cmp | veh && +0xFC != 0 |
| 1671 | CLEAR_CHAR_TASKS | 0x49575F | h | - | intelligence->ClearTasks(true,true) |
| 1672 | TASK_TOGGLE_PED_THREAT_SCANNER | 0x49578F | h, i a, i b, i c | - | h != -1: stack CTaskSimpleTogglePedThreatScanner(a!=0,b!=0,c!=0).ProcessPed(ped); -1: heap task -> active sequence |
| 1673 | POP_CAR_DOOR | 0x495882 | h car, i door, i flag | - | PopDoor(GetCarNodeIndexFromDoor(door), door, flag!=0) |
| 1674 | FIX_CAR_DOOR | 0x4958D2 | h, i door | - | FixDoor(node, door) |
| 1675 | TASK_EVERYONE_LEAVE_CAR | 0x49590E | h car | - | driver: GPST(driverHandle, CTaskComplexLeaveAnyCar(0,true,false)); passengers i < m_nMaxPassengers (+0x488!): rnd(-250,250) + 500*i + 500 |
| 1687 | POP_CAR_PANEL | 0x495AF4 | h, i panel, i flag | - | PopPanel(GetCarNodeIndexFromPanel(panel), panel, flag!=0) |
| 1688 | FIX_CAR_PANEL | 0x495B44 | h, i panel | - | FixPanel(node, panel) |
| 1689 | FIX_CAR_TYRE | 0x495B80 | h car, i tyre | - | FixTyre(tyre) |
| 1690 | ATTACH_OBJECT_TO_OBJECT | 0x495BAF | h, h, f x,y,z, f rx,ry,rz | - | as 1665 |
| 1691 | ATTACH_OBJECT_TO_CHAR | 0x495C6A | h obj, h ped, f x,y,z, f rx,ry,rz | - | as 1665 |
| 1698 | GET_CAR_SPEED_VECTOR | 0x495D27 | h | 3 f | m_vecMoveSpeed * 50.0 |
| 1699 | GET_CAR_MASS | 0x495DAD | h | 1 f | m_fMass |

No-ops: 1633 BREAKPOINT. No `plugin::Call` stubs in g16 (every callee already ported; CTaskSimpleTriggerLookAt is being completed by another agent, see the report).
