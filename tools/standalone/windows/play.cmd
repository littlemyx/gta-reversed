@echo off
chcp 65001 >nul
setlocal EnableExtensions
title GTA SA (standalone)
rem Launcher for a real play session of the standalone build (sound ON, normal user settings).
rem Needs NOTHING of the original gta_sa.exe (no original_data.*, no data_pointers.bin, no gta_sa.exe): the play dir = gta_reversed.exe + the Steam assets (slice G1).
rem Lives in the play data dir C:\gta-play next to gta_reversed.exe (a snapshot of the build + links to the Steam data).
rem   play.cmd              exclusive fullscreen, like the original
rem   play.cmd windowed     window 1280x720 (desktop resolution is not switched)
rem   play.cmd novideos    skip the intro videos (they play by default, like the original: Logo.mpg + GTAtitles.mpg, ~105 s, any key skips; `videos` is accepted and does nothing)
rem   (arguments can be combined, e.g. play.cmd windowed novideos)
rem Env overrides (for tests only): PLAY_MUTE=1 (silent run), PLAY_NOPAUSE=1 (no "press any key" at the end), PLAY_WINDOW=WxH, PLAY_VIDEOS=0 (skip the videos).

set "PLAYDIR=%~dp0"
if "%PLAYDIR:~-1%"=="\" set "PLAYDIR=%PLAYDIR:~0,-1%"
set "LOCKROOT=C:\gta-scratch"
set "LOCK=%LOCKROOT%\gpu.lock"
set "HAVELOCK=0"

if not exist "%PLAYDIR%\gta_reversed.exe" (
  echo Не найден %PLAYDIR%\gta_reversed.exe
  goto :end
)

rem --- GPU lock shared with the agents' soaks (mkdir = acquire, rd = release). The game is single-instance.
if not exist "%LOCKROOT%" md "%LOCKROOT%" >nul 2>&1
set "WAITED=0"
:acquire
md "%LOCK%" >nul 2>&1
if errorlevel 1 (
  if "%WAITED%"=="0" echo Агенты сейчас запускают игру, жду…
  set "WAITED=1"
  ping -n 6 127.0.0.1 >nul
  goto :acquire
)
set "HAVELOCK=1"
> "%LOCK%\owner" echo USER-PLAY %DATE% %TIME%
if "%WAITED%"=="1" echo Очередь освободилась, запускаю.

rem --- environment: sound ON, no test-only switches
if not defined PLAY_MUTE set "PLAY_MUTE=0"
set "NOTSA_STANDALONE_MUTE=%PLAY_MUTE%"
for %%V in (NOTSA_STANDALONE_INPUT NOTSA_STANDALONE_SCREENSHOT NOTSA_STANDALONE_SCREENSHOT_MAX NOTSA_STANDALONE_SCREENSHOT_MS
            NOTSA_STANDALONE_SCREENSHOT_ARM NOTSA_STANDALONE_SCREENSHOT_BURST NOTSA_STANDALONE_SKIP_VIDEOS NOTSA_STANDALONE_VIDEOMODE
            NOTSA_STANDALONE_MEMLOG NOTSA_STANDALONE_ALLOCTRACE NOTSA_STANDALONE_NOPRESENT NOTSA_STANDALONE_NO_ABORT_TRACE
            NOTSA_STANDALONE_SAMPLER NOTSA_STANDALONE_WINDOWED NOTSA_STANDALONE_WINDOWED_FOCUS NOTSA_AUDIO_TAP NOTSA_AUDIO_TAP_RAW NOTSA_AUDIO_STATS NOTSA_AUDIO_STATS_EVERY
            NOTSA_VERIFY_GLOBALS NOTSA_VERIFY_GLOBALS_EXIT NOTSA_VERIFY_GLOBALS_AFTER NOTSA_IMAGE_CENSUS NOTSA_POISON_CONVERTED
            RW_EXE_ORACLE) do set "%%V="

set "WMODE=полный экран"
for %%A in (%*) do (
  if /i "%%~A"=="windowed" if not defined PLAY_WINDOW set "PLAY_WINDOW=1280x720"
  if /i "%%~A"=="videos" set "PLAY_VIDEOS=1"
  if /i "%%~A"=="novideos" set "PLAY_VIDEOS=0"
)
rem videos are ON by default (BUG-VIDEO: the WM_GRAPHNOTIFY of the DirectShow graph now reaches the game, the intro ends by itself); only an explicit 0 / novideos skips them
set "VMODE=видеозаставки включены (любая клавиша - пропустить)"
if "%PLAY_VIDEOS%"=="0" (
  set "VMODE=видеозаставки пропускаются"
  set "NOTSA_STANDALONE_SKIP_VIDEOS=1"
)
if defined PLAY_WINDOW (
  set "NOTSA_STANDALONE_WINDOWED=%PLAY_WINDOW%"
  set "WMODE=окно %PLAY_WINDOW%"
)

rem --- keep the previous log, the game rewrites standalone.log on start
if exist "%PLAYDIR%\standalone.log" move /y "%PLAYDIR%\standalone.log" "%PLAYDIR%\standalone.prev.log" >nul 2>&1

echo.
if "%NOTSA_STANDALONE_MUTE%"=="1" (set "SMODE=звук ВЫКЛЮЧЕН (тестовый режим)") else (set "SMODE=звук включён")
echo Запускаю GTA SA (standalone): %WMODE%, %SMODE%, %VMODE%.
echo Окно с этим текстом не закрывайте крестиком, пока идёт игра: выходите через меню игры.
echo.

rem start /wait: gta_reversed.exe is a GUI program, plain invocation would not wait for it.
start "GTA SA" /wait /d "%PLAYDIR%" "%PLAYDIR%\gta_reversed.exe"
set "RC=%errorlevel%"

rem --- release the lock as soon as the game is gone (also after a crash)
call :release

for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"`) do set "STAMP=%%i"
echo.
echo Игра завершилась, код выхода: %RC%
echo Лог: %PLAYDIR%\standalone.log
if not "%RC%"=="0" (
  if exist "%PLAYDIR%\standalone.log" (
    copy /y "%PLAYDIR%\standalone.log" "%PLAYDIR%\crash-%STAMP%.log" >nul
    echo Код не нулевой - лог сохранён: %PLAYDIR%\crash-%STAMP%.log
  ) else (
    echo Код не нулевой, но standalone.log не найден.
  )
)

:end
if not defined PLAY_NOPAUSE pause
endlocal & exit /b %RC%

:release
if "%HAVELOCK%"=="1" (
  rd /s /q "%LOCK%" >nul 2>&1
  set "HAVELOCK=0"
)
exit /b 0
