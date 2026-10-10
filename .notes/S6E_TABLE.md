# S6-E table, part 1 (g18, ids 1800..1899, 60 commands) - file source/game_sa/Scripts/Commands/Ported/Group18.cpp

Exe group processor 0x46D050 (table 0x46EDB4); every case ends xor al,al (OR_CONTINUE). [O] = oracle-tested (3000 random cases, bit-exact; harness scratchpad/s6e-fable-q4/script_oracle_g18_test.cpp, build ob.sh, run run.sh). GPST = GivePedScriptedTask.

| id | name | case | in | out | notes |
|---|---|---|---|---|---|
| 1800 | CLEAR_CHAR_DECISION_MAKER_EVENT_RESPONSE | 0x46D08E | h dm(thing 7), i event | - | idx in [0,20) -> GetInstance()->FlushDecisionMakerEventResponse. 1865 (CLEAR_GROUP_..., 0x46E4B4) jumps into it |
| 1801 | ADD_CHAR_DECISION_MAKER_EVENT_RESPONSE | 0x46D0D3 | dm, event, task, f p3..p6, i p7, p8 | - | chances={p6,p5,p3,p4} flags={p8,p7} (stack-layout derived); 1866 same (0x46E4CB). not oracle-tested (GetInstance not runnable in harness) |
| 1802 | TASK_PICK_UP_OBJECT | 0x46D166 | (member reads) | - | S.ScriptTaskPickUpObject(cmd) |
| 1803 | DROP_OBJECT | 0x46D175 | ped, flag | - | GetTaskHold(false)?->DropEntity(ped, flag!=0) [O] |
| 1804 | EXPLODE_CAR_IN_CUTSCENE | 0x46D1DB | car | - | bCanBeDamaged=1; BlowUpCar(null,true) (vslot 0xA4) [O] |
| 1806 | PLANE_ATTACK_PLAYER | 0x46D24B | car, i, f | - | mission 35 unless 57/58; float @+0x9B0 [O] |
| 1807 | PLANE_FLY_IN_DIRECTION | 0x46D290 | car, f1,f2,f3 | - | mission 36; +0x9B4=+0x9B8=f1*0.0174532924; +0x9B0=f2; +0x9A8=f3 [O] |
| 1808 | PLANE_FOLLOW_ENTITY | 0x46D304 | car, ped, car2, f | - | mission 37; target(+0x41C) ref update; engine on; +0x9B0=f [O] |
| 1811 | TASK_DRIVE_BY | 0x46D3A7 | ped, targetPed, targetCar, x,y,z, f, i style, i seatRHS, i freq | - | skip if primary task is gang driveby at same target ped; CTaskSimpleGangDriveBy (+0xE=1); GPST |
| 1812 | SET_CAR_STAY_IN_SLOW_LANE | 0x46D4FA | car, flag | - | autopilot carCtrlFlags bit 0x10 [O] |
| 1813 | TAKE_REMOTE_CONTROL_OF_CAR | 0x46D545 | i, car | - | CRemote::TakeRemoteControlOfCar (0x45AD40, ported+hooked in this batch) |
| 1814 | IS_CLOSEST_OBJECT_OF_TYPE_SMASHED_OR_DAMAGED | 0x46D56F | x,y,z,r,model,smashed,damaged | cmp | cmp|FindNearestObjectOfType; bIsBroken / renderDamaged||!visible [O] |
| 1822 | GET_OBJECT_HEALTH | 0x46D6F4 | obj | 1 i | _ftol2 (low dword) of float @+0x154 [O] |
| 1823 | SET_OBJECT_HEALTH | 0x46D71E | obj, i | - | (float)i [O] |
| 1827 | BREAK_OBJECT | 0x46D754 | obj, smash | - | glass: WindowRespondsToCollision(99999.9,0,pos,false); else g_breakMan.Add + flag changes + DeleteRwObject |
| 1828 | HELI_ATTACK_PLAYER | 0x46D8A7 | car, i, f | - | mission 23; +0x9B0=f; cruise=100 [O] |
| 1830 | HELI_FOLLOW_ENTITY | 0x46D8F3 | car, ped, car2, f | - | mission 39; target ref; +0x9B0; cruise 100 [O] |
| 1831 | POLICE_HELI_CHASE_ENTITY | 0x46D994 | same | - | mission 40 [O] |
| 1833 | TASK_USE_MOBILE_PHONE | 0x46DA35 | ped, flag | - | flag>0: new CTaskComplexUseMobilePhone(-1) (ctor 0x6348A0 = plugin stub, class not ported); else quit existing task |
| 1834 | TASK_WARP_CHAR_INTO_CAR_AS_DRIVER | 0x46DAE8 | ped, car | - | CTaskSimpleCarSetPedInAsDriver(car, warp=true); GPST |
| 1835 | TASK_WARP_CHAR_INTO_CAR_AS_PASSENGER | 0x46DB5D | ped, car, seat | - | door = seat>=0 ? ComputeTargetDoorToEnterAsPassenger : 0; warp=true |
| 1836 | SWITCH_COPS_ON_BIKES | 0x46DBE9 | flag | - | DisableCopBikes(flag==0) [O] |
| 1837/1838 | IS_FLAME_IN_ANGLED_AREA_2D/3D | 0x46DC1D | (member) | - | S.FlameInAngledAreaCheckCommand(cmd) |
| 1840 | DAMAGE_CAR_PANEL | 0x46DCAD | car, i | - | ApplyDamage(car, i+0xB, 150.0f, 1.0f) [O] |
| 1841 | SET_CAR_ROLL | 0x46DCF0 | car, f | - | pos=P; SetRotateZ(heading); RotateY(f*deg2rad); pos+=P. [O] with CMatrix::RotateY patched (port differs from exe in 59% of cases!) |
| 1842 | SUPPRESS_CAR_MODEL | 0x46DD76 | model | - | 0x46B1A0; BUG kept: full list writes index 40 (fix under IsFixBugs) [O] |
| 1843 | DONT_SUPPRESS_CAR_MODEL | 0x46DD95 | model | - | 0x46A7E0 [O] |
| 1844 | DONT_SUPPRESS_ANY_CAR_MODELS | 0x46DDB4 | - | - | fill 40 ints with -1 [O] |
| 1847 | IS_CHAR_HOLDING_OBJECT | 0x46DE3E | ped |  | obj|cmp|held-entity logic; pool scan if no ped [O] |
| 1851 | SET_CAR_CAN_GO_AGAINST_TRAFFIC | 0x46DF7A | car, flag | - | bCantGoAgainstTraffic = flag==0 [O] |
| 1852 | DAMAGE_CAR_DOOR | 0x46DFC5 | car, i | - | ApplyDamage(car, i+5, 150, 1) [O] |
| 1854 | GET_RANDOM_CAR_IN_SPHERE_NO_SAVE | 0x46E009 | x,y,z,r,model | 1 h | pool scan last->first [O] |
| 1855 | GET_RANDOM_CHAR_IN_SPHERE | 0x46E172 | x,y,z,r,civ,gang,crim | 1 h | pool scan; mission ped; cleanup [O] |
| 1857 | HAS_CHAR_BEEN_ARRESTED | 0x46E348 | ped | cmp | m_nPedState==0x3F [O] |
| 1858 | SET_PLANE_THROTTLE | 0x46E37A | car, f | - | float @+0x998 [O] |
| 1859 | HELI_LAND_AT_COORDS | 0x46E3A8 | car, 5 f | - | TellHeliToGoToCoors; mission 47 [O] |
| 1861 | PLANE_STARTS_IN_AIR | 0x46E411 | car | - | IsAlreadyFlying; model 0x208 -> word @+0x86C=0 [O] |
| 1862 | SET_RELATIONSHIP | 0x46E450 | rel, type1, type2 | - | SetPedTypeAsAcquaintance(rel,type1,GetPedFlag(type2)) [O] |
| 1863 | CLEAR_RELATIONSHIP | 0x46E482 | same | - | Clear... [O] |
| 1865/1866 | CLEAR/ADD_GROUP_DECISION_MAKER_EVENT_RESPONSE | 0x46E4B4/0x46E4CB | as 1800/1801 | - | shared handlers |
| 1868 | TASK_USE_ATTRACTOR | 0x46E675 | ped, attractor(thing 6) | - | idx in [0,0x40): CTaskComplexUseEffect(&effects[idx], null) |
| 1869 | TASK_SHOOT_AT_CHAR | 0x46E6FF | ped, target, time | - | CTaskSimpleGunControl(target,0,0,3,1,time) |
| 1870 | SET_INFORM_RESPECTED_FRIENDS | 0x46E77C | ped, f, i | - | intel +0xC8=f, +0xC4=i [O] |
| 1871 | IS_CHAR_RESPONDING_TO_EVENT | 0x46E7BC | ped, event | cmp | intelligence->IsRespondingToEvent |
| 1872 | SET_OBJECT_VISIBLE | 0x46E802 | obj, flag | - | entity bIsVisible [O] |
| 1873 | TASK_FLEE_CHAR_ANY_MEANS | 0x46E84A | ped, target, f safe, i flee, i attack, i shoot, i recover, f steal | - | CTaskComplexFleeAnyMeans(.., 1000 (0x86F678), global float @0xC18CF0) |
| 1876 | FLUSH_PATROL_ROUTE | 0x46E92A | - | - | route @0xC18DB8 count=0 [O] |
| 1877 | EXTEND_PATROL_ROUTE | 0x46E93B | x,y,z, text24, text16 | - | label "NONE\0" -> empty anim; AddNode (0x46AE80) [O] |
| 1882 | PLAY_OBJECT_ANIM | 0x46E9FC | obj, text24, text16, f, i, i | cmp | BlendAnimation if clump |
| 1883 | SET_RADAR_ZOOM | 0x46EAE7 | i | - | byte @0xA444A3 [O] |
| 1884 | DOES_BLIP_EXIST | 0x46EB03 | blip | cmp | GetActualBlipArrayIndex != -1 [O] |
| 1885 | LOAD_PRICES | 0x46EB37 | text16 | - | CShopping::LoadPrices |
| 1886 | LOAD_SHOP | 0x46EB59 | text16 | - | CShopping::LoadShop |
| 1887 | GET_NUMBER_OF_ITEMS_IN_SHOP | 0x46EB7B | - | 1 i | ms_numItemsInShop [O] |
| 1888 | GET_ITEM_IN_SHOP | 0x46EB97 | i | 1 i | ms_shopContents[i] unchecked [O] |
| 1889 | GET_PRICE_OF_ITEM | 0x46EBAF | item | 1 i | CShopping::GetPrice [O] |
| 1890 | TASK_DEAD | 0x46EBDB | ped | - | CTaskComplexDie(0,0,0xF,4.0f,...) |
| 1891 | SET_CAR_AS_MISSION_CAR | 0x46EC43 | car | - | cleanup: createdBy 1/3 -> MISSION + AddEntityToList [O] |


# S6-E table, g19 (ids 1900..1999, 59 commands) - file source/game_sa/Scripts/Commands/Ported/Group19.cpp

Exe group processor 0x46B460 (jump table 0x46CBEC). All cases end `xor al, al` (OR_CONTINUE). "cmp" = compare flag. [O] = oracle-tested (3000 random cases, bit-exact vs the exe).

| id | name | case | in | out | notes |
|---|---|---|---|---|---|
| 1903 | IS_MESSAGE_BEING_DISPLAYED | 0x46B571 | - | cmp | BriefMessages[0].Text (0xC1A7F0) != 0 [O] |
| 1904 | SET_CHAR_IS_TARGET_PRIORITY | 0x46B597 | h ped, i flag | - | ped flags dword +0x470 bit 29; no null check [O] |
| 1905 | CUSTOM_PLATE_DESIGN_FOR_NEXT_CAR | 0x46B5E8 | i model, i design | - | ms_modelInfoPtrs[model] unchecked; type 6 and +0x24 (m_pPlateMaterial) set => +0x31 (m_nPlateType) = byte [O] |
| 1906 | TASK_GOTO_CAR | 0x46B630 | h ped, h car, i time, f radius | - | time<0 -> 50000; CTaskComplexSeekEntityStandard(car,time,1000,radius,2,2,true,true) 0x46AC10; GPST |
| 1910 | REQUEST_IPL | 0x46B6D4 | text(18) | - | FindIplSlot -> RequestIplAndIgnore |
| 1911 | REMOVE_IPL | 0x46B6FC | text(18) | - | FindIplSlot -> RemoveIplAndIgnore |
| 1912 | REMOVE_IPL_DISCREETLY | 0x46B724 | text(18) | - | FindIplSlot -> RemoveIplWhenFarAway |
| 1914 | SET_CHAR_RELATIONSHIP | 0x46B74C | h ped, i id, i pedType | - | m_acquaintance(+0x4E0).SetAsAcquaintance(id, GetPedFlag(type)) [O] |
| 1915 | CLEAR_CHAR_RELATIONSHIP | 0x46B791 | h ped, i id, i pedType | - | ClearAsAcquaintance(id, GetPedFlag(type)) [O] |
| 1916 | CLEAR_ALL_CHAR_RELATIONSHIPS | 0x46B7D7 | h ped, i id | - | ClearAcquaintances(id) [O] |
| 1917 | GET_CAR_PITCH | 0x46B80C | h car | 1 f | CAutomobile::GetCarPitch*57.29578f; <0 test on the x87 register (unrounded), +360 on the stored float, >360 => -360 [O] |
| 1918 | GET_AREA_VISIBLE | 0x46B88C | - | 1 i | CGame::currArea [O] |
| 1920 | HELI_KEEP_ENTITY_IN_VIEW | 0x46B8A6 | h heli, h ped, h car, f a, f b | - | target = ped (handle>=0) overwritten by car (handle>=0); mission 0x33 unless 0x39/0x3A; +0x41C ref swap (no null check); +0x9B0=b; +0x3D0=100; +0x3D8=(u8)ftol(a) [O] |
| 1921 | GET_WEAPONTYPE_MODEL | 0x46B960 | i type | 1 i | GetWeaponInfo(type,1)->+0xC [O] |
| 1922 | GET_WEAPONTYPE_SLOT | 0x46B992 | i type | 1 i | GetWeaponInfo(type,1)->+0x14 [O] |
| 1923 | GET_SHOPPING_EXTRA_INFO | 0x46B9C5 | i key, i idx | 1 i | CShopping::GetExtraInfo [O] |
| 1926 | GET_NUMBER_OF_FIRES_IN_AREA | 0x46BA33 | 6 f | 1 i | per-axis ordered swap (a>b), gFireManager.GetNumFiresInArea(x1,y1,z1,x2,y2,z2) [O] |
| 1928 | ATTACH_WINCH_TO_HELI | 0x46BB10 | h heli, i arg | - | InitWinch(arg) [O] |
| 1929 | RELEASE_ENTITY_FROM_WINCH | 0x46BB3F | h heli | - | ReleasePickedUpEntityWithWinch [O] |
| 1930 | GET_TRAIN_CARRIAGE | 0x46BB68 | h train, i idx(u8) | 1 h | FindCarriage -> GetRef, -1 when null [O] |
| 1931 | GRAB_ENTITY_ON_WINCH | 0x46BBD3 | h heli | 3 h | QueryPickedUpEntityWithWinch; type 2 -> car, 3 -> ped, 4 -> object, others/none -> -1 [O] |
| 1932 | GET_NAME_OF_ITEM | 0x46BC30 | i key, var | 8 bytes | GetNameTag; strncpy(var, tag, 8); var fetched with GetPointerToScriptVariable(2) after the key [O] |
| 1935 | TASK_CLIMB | 0x46BC65 | h ped, i flag | - | CTaskComplexClimb, byte +0x10 (m_UsePlayerLaunchForce) = flag != 0; GPST |
| 1936 | BUY_ITEM | 0x46BCC6 | i key | - | CShopping::Buy(key, 0) [O] |
| 1939 | STORE_CLOTHES_STATE | 0x46BD15 | - | - | CShopping::StoreClothesState [O] |
| 1940 | RESTORE_CLOTHES_STATE | 0x46BD21 | - | - | CShopping::RestoreClothesState [O] |
| 1942 | GET_ROPE_HEIGHT_FOR_OBJECT | 0x46BD2D | h obj | 1 f | CObject::GetRopeHeight [O] |
| 1943 | SET_ROPE_HEIGHT_FOR_OBJECT | 0x46BD65 | h obj, f h | - | SetRopeHeight [O] |
| 1944 | GRAB_ENTITY_ON_ROPE_FOR_OBJECT | 0x46BD94 | h obj | 3 h | GetObjectCarriedWithRope, same dispatch as 1931 [O] |
| 1945 | RELEASE_ENTITY_FROM_ROPE_FOR_OBJECT | 0x46BE30 | h obj | - | ReleaseObjectCarriedWithRope [O] |
| 1952 | PERFORM_SEQUENCE_TASK_FROM_PROGRESS | 0x46BEA0 | h ped, i seq, i progress, i end | - | idx = GetActualScriptThingIndex(seq,4); only 0<=idx<64; CTaskComplexUseSequence(idx), +0x10=progress, +0x14=end; GPST |
| 1953 | SET_NEXT_DESIRED_MOVE_STATE | 0x46BF39 | i state | - | 0x8D237C = state [O] |
| 1955 | TASK_GOTO_CHAR_AIMING | 0x46BF53 | h ped, h target, f seekR, f aimR | - | CTaskComplexSeekEntityAiming(target, seekR, aimR) 0x694B90; GPST |
| 1956 | GET_SEQUENCE_PROGRESS_RECURSIVE | 0x46BFD8 | h ped | 2 i | status(0x618)!=0: primary task +0x10 (unchecked), virtual GetSubTask, type 0x113 -> its +0x10; default -1,-1 [O] |
| 1957 | TASK_KILL_CHAR_ON_FOOT_TIMED | 0x46C04F | h ped, h target, i time | - | CTaskComplexKillPedOnFoot(target,time,0,0,0,1) 0x620E30; GPST |
| 1958 | GET_NEAREST_TAG_POSITION | 0x46C0AF | 3 f | 3 f | GetNearestTag(pos): entity position, else (-4000)x3 [O] |
| 1959 | TASK_JETPACK | 0x46C149 | h ped | - | CTaskSimpleJetPack(nullptr,10,0,nullptr); GPST |
| 1960 | SET_AREA51_SAM_SITE | 0x46C1A7 | i v | - | bArea51SamSiteDisabled (0xBB4A72) = (v == 0) [O] |
| 1961 | IS_CHAR_IN_ANY_SEARCHLIGHT | 0x46C1C7 | h ped | 1 i + cmp | inlined 0x493960: 8 lights, used && IsPointInsideLitEllipse(pos): id<<16\|slot else -1; store then flag [O] |
| 1963 | IS_TRAILER_ATTACHED_TO_CAB | 0x46C22E | h trailer, h cab | cmp | trailer.+0x4C4==cab / !=null; cab only: cab.+0x4C8 != null; both null derefs null (as the exe) [O] |
| 1964 | DETACH_TRAILER_FROM_CAB | 0x46C2AF | h trailer, h cab | - | trailer: vslot 62 (BreakTowLink); cab only: its +0x4C8 trailer; both null derefs null [O] |
| 1968 | GET_LOADED_SHOP | 0x46C364 | var | 8 bytes | strncpy(tmp,ms_shopLoaded,8); MakeUpperCase; var = GetPointerToScriptVariable(2); strncpy(var,tmp,8) [O] |
| 1969 | GET_BEAT_PROXIMITY | 0x46C3A2 | i off | 3 i | GetBeatInfo; not present -> -1x3; off<0: [off+10], >0: [off+9] (unchecked), BeatNumber; 0: special pick of entry 9/10 [O] |
| 1971 | SET_GROUP_DEFAULT_TASK_ALLOCATOR | 0x46C4B7 | i group, i type | - | idx=GetActualScriptThingIndex(group,8) in [0,8): groups[idx].intelligence.SetDefaultTaskAllocatorType [O] |
| 1979 | ACTIVATE_HELI_SPEED_CHEAT | 0x46C552 | h heli, i v | - | +0x3D9 (m_ucHeliSpeedMult) = (u8)v [O] |
| 1980 | TASK_SET_CHAR_DECISION_MAKER | 0x46C580 | h ped, i dm | - | dm=-1 or GetActualScriptThingIndex(dm,7); ped!=-1: SetPedDecisionMakerType(dm); -1: CTaskSimpleSetCharDecisionMaker(dm) via GPST [O ped path] |
| 1981 | DELETE_MISSION_TRAIN | 0x46C615 | h train | - | non-null: RemoveOneMissionTrain [O] |
| 1982 | MARK_MISSION_TRAIN_AS_NO_LONGER_NEEDED | 0x46C647 | h train | - | non-null: ReleaseOneMissionTrain [O] |
| 1983 | SET_BLIP_ALWAYS_DISPLAY_ON_ZOOMED_RADAR | 0x46C67A | i blip, i flag(u8) | - | SetBlipAlwaysDisplayInZoom (callee uses bit 0 only) [O] |
| 1984 | REQUEST_CAR_RECORDING | 0x46C6A1 | i n | - | RequestRecordingFile [O] |
| 1985 | HAS_CAR_RECORDING_BEEN_LOADED | 0x46C6C0 | i n | cmp | HasRecordingFileBeenLoaded [O] |
| 1987 | GET_OBJECT_QUATERNION | 0x46C6E4 | h obj | 4 f | CQuaternion::Set(*GetModellingMatrix()) (x,y,z,w) |
| 1988 | SET_OBJECT_QUATERNION | 0x46C747 | h obj, 4 f | - | q.Get(rw); CMatrix(&rw,false); SetMatrix; SetPosn(old pos) |
| 1989 | GET_VEHICLE_QUATERNION | 0x46C7F7 | h car | 4 f | as 1987 |
| 1990 | SET_VEHICLE_QUATERNION | 0x46C85A | h car, 4 f | - | as 1988 |
| 1991 | SET_MISSION_TRAIN_COORDINATES | 0x46C928 | h train, 3 f | - | SetNewTrainPosition(train, pos) (train passed even if null) [O] |
| 1993 | TASK_COMPLEX_PICKUP_OBJECT | 0x46C988 | h ped, h obj | - | CTaskComplexGoPickUpEntity(obj, 0x51); GPST |
| 1995 | LISTEN_TO_PLAYER_GROUP_COMMANDS | 0x46C9F2 | h ped, i v | - | ped flags dword +0x474 bit 5 = (v == 0) [O] |
| 1997 | TASK_CHAR_SLIDE_TO_COORD | 0x46CA88 | h ped, 3 f, f heading, f speed | - | angle=(float)(heading*0.017453292f); speed<0 -> 0.1f; Sequence{GoToPointAndStandStill(WALK,pos,0.5,2,false,false), SlideToCoord(pos,angle,speed)}; GPST |
