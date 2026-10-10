# S6-C g13..g15 (ids 1303..1592) ported script handlers

Group processors: g13 0x48CDD0 (ids 1301..1397), g14 0x48EAA0 (1403..1499), g15 0x490DB0 (1500..1593). No no-ops, no plugin::Call stubs. ORACLE = bit-exact vs the exe group processor (tests/standalone/script_oracle_g13_15_test.cpp).

## Non-task commands (Group13_15.cpp)

| id | name | exe case | params in | results out | notes |
|---|---|---|---|---|---|
| 1303 | CREATE_LOCKED_PROPERTY_PICKUP | 0x48CDF9 | x,y,z,label8 | 1 handle | z<=-100 => groundZ+0.5; MI_PICKUP_PROPERTY, type 0x11, text=TheText.Get; dead peek+GetActualPickupIndex |
| 1304 | CREATE_FORSALE_PROPERTY_PICKUP | 0x48CEDB | x,y,z,price,label8 | 1 handle | as above, FORSALE model, type 0x12, ammo=price |
| 1305 | FREEZE_CAR_POSITION | 0x48CFA5 | car,int | - | flags 0x200C, SkipPhysics, speeds 0 / clear |
| 1308 | HAS_CAR_BEEN_DAMAGED_BY_CHAR | 0x48D0D2 | car,char | flag | -1 = any ped; else ped or (bInVehicle && ped's vehicle); ped deref unchecked |
| 1309 | HAS_CAR_BEEN_DAMAGED_BY_CAR | 0x48D15B | car,car | flag | -1 = any vehicle |
| 1310 | GET_RADIO_CHANNEL | 0x48D1C6 | - | 1 int | (int8)station-1 |
| 1342 | GET_RANDOM_CAR_OF_TYPE_IN_AREA_NO_SAVE | 0x48D278 | x1,y1,x2,y2,model | 1 handle | pool scan from the end; sets bHasBeenOwnedByPlayer; no mission cleanup |
| 1343 | SET_CAN_BURST_CAR_TYRES | 0x48D356 | car,int | - | bTyresDontBurst=!arg |
| 1345 | FIRE_HUNTER_GUN | 0x48D3AA | heli | - | CWeapon(MINIGUN,5000).FireInstantHit, gunshell, audio 0x95; 150ms throttle on +0x4D4 |
| 1359 | CLEAR_CAR_LAST_DAMAGE_ENTITY | 0x48D5E8 | car | - | null skipped |
| 1360 | FREEZE_OBJECT_POSITION | 0x48D621 | obj,int | - | flags 0x2004 |
| 1365 | REMOVE_WEAPON_FROM_CHAR | 0x48D673 | ped,int | - | ClearWeapon, no null check |
| 1380 | MAKE_HELI_COME_CRASHING_DOWN | 0x48D903 | heli | - | mission 0x3A unless 0x39/0x3A |
| 1381 | ADD_EXPLOSION_NO_SOUND | 0x48D94A | x,y,z,type | - | AddExplosion(null,null,type,pos,0,0,-1,0) |
| 1382 | SET_OBJECT_AREA_VISIBLE | 0x48D9B2 | obj,int | - | area code byte |
| 1386 | WAS_CUTSCENE_SKIPPED | 0x48DA3C | - | flag | ms_wasCutsceneSkipped |
| 1390 | DOES_VEHICLE_EXIST | 0x48DAF9 | car | flag |  |
| 1392 | ADD_SHORT_RANGE_SPRITE_BLIP_FOR_CONTACT_POINT | 0x48DB3D | x,y,z,sprite | 1 blip | SetShortRangeCoordBlip(5,pos,2,3,name) |
| 1396 | FREEZE_CAR_POSITION_AND_DONT_LOAD_COLLISION | 0x48DC3F | car,int | - | mission cleanup: Remove/flag/Add |
| 1415 | SET_LOAD_COLLISION_FOR_CAR_FLAG | 0x48EBF4 | car,int | - | inverse of the above, AddToMovingList |
| 1420 | GET_PROGRESS_PERCENTAGE | 0x48EDA0 | - | 1 float |  |
| 1428 | SET_VEHICLE_TO_FADE_IN | 0x48EDBB | car,alpha | - | SetClumpAlpha |
| 1429 | REGISTER_ODDJOB_MISSION_PASSED | 0x48EDEF | - | - | stats 0x93,0xB1,0xB0; LastMissionPassedTime |
| 1434 | IS_AUSTRALIAN_GAME | 0x48EE42 | - | flag false | ORACLE |
| 1436 | SET_ONSCREEN_COUNTER_FLASH_WHEN_FIRST_DISPLAYED | 0x48EE90 | globalvar,int | - | var read first |
| 1437 | SHUFFLE_CARD_DECKS | 0x48EED9 | int | - | rand() & 0xFFFF idiom; ORACLE |
| 1438 | FETCH_NEXT_CARD | 0x48EFAD | - | 1 int | ORACLE |
| 1441 | ADD_TO_OBJECT_ROTATION_VELOCITY | 0x48F07D | obj,x,y,z | - | 0.02f scale, 0x59C790 rotate, SetIsStatic(false)+AddToMovingList if static |
| 1442 | SET_OBJECT_ROTATION_VELOCITY | 0x48F17E | obj,x,y,z | - | timestep; *0.02f |
| 1443 | IS_OBJECT_STATIC | 0x48F260 | obj | flag |  |
| 1444 | GET_ANGLE_BETWEEN_2D_VECTORS | 0x48F2A2 | 4 floats | 1 float | x87, CRT acos sequence (AcosCrt); ORACLE |
| 1445 | DO_2D_RECTANGLES_COLLIDE | 0x48F339 | 8 floats | flag | ORACLE |
| 1446 | GET_OBJECT_ROTATION_VELOCITY | 0x48F461 | obj | 3 floats | 0x59C810; *50 |
| 1447 | ADD_VELOCITY_RELATIVE_TO_OBJECT_VELOCITY | 0x48F4E9 | obj,x,y,z | - | 0x59C910/0x59C730 originals; dead second static test kept |
| 1448 | GET_OBJECT_SPEED | 0x48F73F | obj | 1 float |  |
| 1456 | GET_2D_LINES_INTERSECT_POINT | 0x48F830 | 8 floats | flag + 2 floats | flag set before the stores; ORACLE |
| 1515 | START_PLAYBACK_RECORDED_CAR | 0x490FB0 | car,int | - | (veh,file,false,false) |
| 1516 | STOP_PLAYBACK_RECORDED_CAR | 0x490FE6 | car | - | null skipped |
| 1517 | PAUSE_PLAYBACK_RECORDED_CAR | 0x491018 | car | - |  |
| 1518 | UNPAUSE_PLAYBACK_RECORDED_CAR | 0x491043 | car | - |  |
| 1521 | SET_CAR_ESCORT_CAR_LEFT | 0x49106E | car,car | - | +0x41C target + RegisterReference; mission 0x1D unless 0x39/0x3A |
| 1522 | SET_CAR_ESCORT_CAR_RIGHT | 0x4910CE | car,car | - | 0x1E |
| 1523 | SET_CAR_ESCORT_CAR_REAR | 0x49112E | car,car | - | 0x1F |
| 1524 | SET_CAR_ESCORT_CAR_FRONT | 0x49118F | car,car | - | 0x20 |
| 1531 | IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_2D | 0x491289 | - | - | CharInAngledAreaCheckCommand(cmd), shared with 1537 |
| 1537 | IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_3D | 0x491289 | - | - |  |
| 1540 | GET_HEADING_FROM_VECTOR_2D | 0x49137C | x,y | 1 float | ORACLE |
| 1542 | LOAD_PATH_NODES_IN_AREA | 0x491404 | 4 floats | - | min/max swap |
| 1543 | RELEASE_PATH_NODES | 0x491494 | - | - |  |
| 1550 | IS_PLAYBACK_GOING_ON_FOR_CAR | 0x4915F5 | car | flag |  |
| 1551 | SET_SENSE_RANGE | 0x49162A | ped,float | - | -1 = all PED_MISSION peds of the moving list |
| 1557 | OPEN_SEQUENCE_TASK | 0x49186F | - | 1 handle | GetAvailableSlot (ported, was a stub) |
| 1558 | CLOSE_SEQUENCE_TASK | 0x491908 | seq | - |  |
| 1560 | PERFORM_SEQUENCE_TASK | 0x49194B | ped handle,seq | - | GivePedScriptedTask(CTaskComplexUseSequence) |
| 1563 | CLEAR_SEQUENCE_TASK | 0x491A4A | seq | - |  |
| 1565 | ADD_ATTRACTOR | 0x491ABC | x,y,z,a1,a2,seq | 1 handle |  |
| 1566 | CLEAR_ATTRACTOR | 0x491C33 | handle | - |  |
| 1569 | CREATE_CHAR_AT_ATTRACTOR | 0x491C7F | type,model,attractor,taskCmd | 1 handle |  |
| 1582 | GET_SCRIPT_TASK_STATUS | 0x49220A | ped,int | 1 int | -1 => 7 |
| 1583 | CREATE_GROUP | 0x492258 | int | 1 handle |  |
| 1584 | SET_GROUP_LEADER | 0x4922D8 | group,ped | - |  |
| 1585 | SET_GROUP_MEMBER | 0x4923BA | group,ped | - | group AI events |
| 1586 | REMOVE_GROUP | 0x4926E9 | group | - | player-led group returns before the cleanup removal |

## TASK_* commands (Group13_15_task.cpp)

| id | name | exe case | params in | task class + ctor addr | notes |
|---|---|---|---|---|---|
| 1465 | TASK_PAUSE | 0x48FB0E | ped, int time | CTaskSimplePause 0x48E750 | |
| 1466 | TASK_STAND_STILL | 0x48FB82 | ped, int time | CTaskSimpleStandStill 0x62F310 | -1 => 20000, -2 => (999999, looped=true); blend 8.0 |
| 1467 | TASK_FALL_AND_GET_UP | 0x48FC9D | ped, int dir, int time | CTaskComplexFallAndGetUp(dir,time) 0x678700 | |
| 1468 | TASK_JUMP | 0x48FCE7 | ped, bool | CTaskComplexJump(OK) 0x67A030 | byte +0x10 (m_UsePlayerLaunchForce) = flag != 0 |
| 1469 | TASK_TIRED | 0x48FD4A | ped, int time | CTaskSimpleTired 0x630F20 | |
| 1470 | TASK_DIE | 0x48FD8D | ped | CTaskComplexDie 0x630040 | (0,0,0xF,4.0,0,0,0,FORWARD,0) |
| 1471 | TASK_LOOK_AT_CHAR | 0x48FE0F | ped, char, int time | IKChainManager_c::LookAt 0x618970 / CTaskSimpleTriggerLookAt 0x634440 | ped!=-1 => immediate LookAt (nothing given); -1 => sequence. -1=>20000, -2=>-1. TriggerLookAt class completed (ctor, Clone, GetTaskType, MakeAbortable, ProcessPed) |
| 1472 | TASK_LOOK_AT_VEHICLE | 0x48FF19 | ped, veh, int time | same, bone -1 | -1=>20000, -2=>0x7FFFFFFF |
| 1473 | TASK_SAY | 0x490006 | ped, int ctx | CTaskSimpleSay 0x48E360 | ped!=-1 => SetTaskSecondary(SAY) 0x681B60, else AffectSecondaryBehaviour 0x691270 in sequence; no GivePedScriptedTask |
| 1474 | TASK_SHAKE_FIST | 0x4900B2 | ped | CTaskSimpleShakeFist 0x690B80 | as SAY with PARTIAL_ANIM |
| 1475 | TASK_COWER | 0x490143 | ped | CTaskSimpleCower 0x48DE70 | |
| 1476 | TASK_HANDS_UP | 0x49017F | ped, int time | CTaskSimpleHandsUp 0x48E970 | -1=>20000, -2=>0x7FFFFFFF |
| 1477 | TASK_DUCK | 0x4901D8 | ped, int time | CTaskSimpleDuck(0,len,-1) 0x691FC0 | -2 => len 0, -1 => 20000 |
| 1479 | TASK_USE_ATM | 0x49029B | ped | CTaskSimpleUseAtm 0x48DFE0 | |
| 1480 | TASK_SCRATCH_HEAD | 0x4902D7 | ped | CTaskSimpleScratchHead 0x48DF30 | |
| 1481 | TASK_LOOK_ABOUT | 0x490313 | ped, int time | CTaskSimpleLookAbout 0x48E0A0 | -1=>20000, -2=>0x7FFFFFFF |
| 1482 | TASK_ENTER_CAR_AS_PASSENGER | 0x49036C | ped, veh, int time, int seat | CTaskComplexEnterCarAsPassengerTimed 0x63B030 | door = seat!=-1 ? ComputeTargetDoorToEnterAsPassenger : 0; -2=>-1, -1=>20000; moveState(+0x1C) = [0x8D237C], then [0x8D237C]=6 |
| 1483 | TASK_ENTER_CAR_AS_DRIVER | 0x49046B | ped, veh, int time | CTaskComplexEnterCarAsDriverTimed 0x63AD70 | -2=>-1, -1=>20000; moveState(+0x14) = [0x8D237C], then =6 |
| 1485 | TASK_LEAVE_CAR | 0x490554 | ped, veh | CTaskComplexLeaveCar(veh,0,0,true,false) 0x63B8C0 | |
| 1487 | TASK_LEAVE_CAR_AND_FLEE | 0x4905AC | ped, veh, x,y,z | CTaskComplexLeaveCarAndFlee 0x63BF90 | (veh,&pt,0,0,false) |
| 1489 | TASK_CAR_DRIVE_TO_COORD | 0x490649 | ped, veh, x,y,z, float speed, int arg4, int modelMode, int style | CTaskComplexDriveToPoint 0x63CE00 | veh handle >=0 signed test; z<=-100 => FindGroundZ; modelMode 0=>-1, 1=>0x19F; radius -1.0 |
| 1490 | TASK_CAR_DRIVE_WANDER | 0x490762 | ped, veh, float speed, int style | CTaskComplexCarDriveWander(veh,style,speed) 0x63CB10 | veh handle >=0 signed test |
| 1491 | TASK_GO_STRAIGHT_TO_COORD | 0x4907CE | ped, x,y,z, int ms, int time | CTaskComplexGoToPointAndStandStill 0x668120 / ...Timed 0x6685E0 | -2 => StandStill(0.5,2.0,false,true); -1 => Timed(...,20000); else Timed(...,time) |
| 1492 | TASK_ACHIEVE_HEADING | 0x49090B | ped, float deg | CTaskSimpleAchieveHeading 0x667E20 | deg * 0.017453292f (0x8595EC); (h,0.5,0.2) |
| 1494 | FLUSH_ROUTE | 0x490981 | - | - | route count (0xC18D50) = 0 |
| 1495 | EXTEND_ROUTE | 0x490992 | x,y,z | CPointRoute::AddUnlessFull 0x48E180 | |
| 1496 | TASK_FOLLOW_POINT_ROUTE | 0x4909D4 | ped, int ms, int mode | CTaskComplexFollowPointRoute 0x671510 | (ms,route,mode,0.5,5.0,false,true,true) |
| 1497 | TASK_GOTO_CHAR | 0x490A55 | ped, char, int time, float radius | CTaskComplexSeekEntity<Standard> 0x46AC10 | -2=>-1, -1=>50000; (target,time,1000,radius,2,2,true,true) |
| 1498 | TASK_FLEE_POINT | 0x490AE6 | ped, x,y,z, float radius, int time | CTaskComplexFleePoint 0x65B390 | scream=true |
| 1499 | TASK_FLEE_CHAR | 0x490B7E | ped, char, int dist (fild->float), int time | CTaskComplexFleeEntity 0x65B930 | (target,true,(float)dist,time,1000,1.0) |
| 1500 | TASK_SMART_FLEE_POINT | 0x490DEE | ped, x,y,z, float dist, int time | CTaskComplexSmartFleePoint 0x65BD20 | |
| 1501 | TASK_SMART_FLEE_CHAR | 0x490E63 | ped, char, int dist (fild->float), int time | CTaskComplexSmartFleeEntity 0x65C430 | (target,true,(float)dist,time,1000,[0xC18CF0]) |
| 1502 | TASK_WANDER_STANDARD | 0x490EF7 | ped | CTaskComplexWanderStandard 0x48E4F0 | (WALK(4), rand 0..8, true) |
| 1525 | TASK_FOLLOW_PATH_NODES_TO_COORD | 0x4911EF | ped, x,y,z, int ms, int time | CTaskComplexFollowNodeRoute 0x66EA30 | -1=>50000, -2=>-1; (ms,pt,0.5,3.0,2.0,true,time,true) |
| 1539 | TASK_GO_TO_COORD_ANY_MEANS | 0x4912F4 | ped, x,y,z, int ms, veh | CTaskComplexGoToPointAnyMeans 0x66B790 | veh handle >=0 signed test; radius = [0xC18DB4]; model -1 |
| 1541 | TASK_PLAY_ANIM | 0x4913F5 | - (reads inside) | CRunningScript::PlayAnimScriptCommand 0x470150 | no CollectParameters in the case |
| 1570 | TASK_LEAVE_CAR_IMMEDIATELY | 0x491FFF | ped, veh | CTaskComplexLeaveCar(veh,0,0,false,false) 0x63B8C0 | |
| 1587 | TASK_LEAVE_ANY_CAR | 0x49277A | ped | CTaskComplexLeaveAnyCar(0,true,false) 0x421150 | |
| 1589 | TASK_AIM_GUN_AT_CHAR | 0x49283D | ped, char, int time | CTaskSimpleGunControl 0x61F3F0 | (target,null,null,NONE,1,time); null vec ptrs => zero vectors |
| 1591 | TASK_GO_TO_COORD_WHILE_SHOOTING | 0x4928AD | ped, x,y,z, int ms, float r, float slow, char | CTaskComplexGoToPointShooting 0x668C70 | aimPos (0,0,0) |
| 1592 | TASK_STAY_IN_SAME_PLACE | 0x492975 | ped, bool | CTaskSimpleSetStayInSamePlace 0x62F590 | ped!=-1 => stack task, ProcessPed(ped) 0x62F5E0; -1 => sequence; no GivePedScriptedTask |
