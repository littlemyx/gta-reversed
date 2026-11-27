# usage: powershell -File mouse_sweep.ps1 <standalone.log> <exe-dir-prefix> [dx=100] [n=40] [delayMs=20] [sweeps=3]
# USER-ACTIVITY GUARD: the sweep moves the REAL cursor (mouse_event = hardware-style raw input, the path the scenario guards), so a user at the PC disturbs it. The script therefore watches GetLastInputInfo:
# after each of its own injections it remembers the last-input tick; any later change of that tick that is not ours (the user's mouse / key) or a foreground window that is not the game's, from 3 s before the first sweep to the end of
# the last one, is reported as `USER_ACTIVE ...` lines and a final `SWEEPS_DONE user_active=0|1`; gameplay_regress.sh re-runs the scenario (twice at most) and shows SKIP(user active) when it is still disturbed. Nothing here changes what is injected.
# Side-car of the "input_mouse" scenario: for every `[mark] sweepK` that appears in the game's log, centre the real cursor on the game window and send n raw relative mouse moves of dx pixels (mouse_event).
param($log, $dir, $dx = 100, $n = 40, $delayMs = 20, $sweeps = 3, $maxWaitS = 240)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class GrM { [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
[DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y); [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r); [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
[DllImport("user32.dll")] public static extern bool GetLastInputInfo(ref LII p);
[StructLayout(LayoutKind.Sequential)] public struct LII { public uint cb; public uint t; }
[StructLayout(LayoutKind.Sequential)] public struct R { public int L,T,Rr,B; }
public static uint Last() { LII p = new LII(); p.cb = 8; GetLastInputInfo(ref p); return p.t; } }
"@
[GrM]::SetProcessDPIAware() | Out-Null
$done = @{}
$own = [GrM]::Last()          # last-input tick we know about (ours, or the user's already counted)
$foreign = New-Object System.Collections.ArrayList
$firstMark = $null            # time the first sweep mark was seen
$t0 = Get-Date
function Check-User($what) {
  $t = [GrM]::Last()
  if ($t -ne $script:own) {
    [void]$script:foreign.Add([pscustomobject]@{ at = (Get-Date); what = $what })
    $script:own = $t
  }
}
while (((Get-Date) - $t0).TotalSeconds -lt $maxWaitS -and $done.Count -lt $sweeps) {
  Start-Sleep -Milliseconds 200
  Check-User 'poll'
  $txt = ''
  try { $fs = [System.IO.File]::Open($log, 'Open', 'Read', 'ReadWrite'); $sr = New-Object System.IO.StreamReader($fs); $txt = $sr.ReadToEnd(); $sr.Close(); $fs.Close() } catch { continue }
  for ($k = 1; $k -le $sweeps; $k++) {
    if ($done.ContainsKey($k) -or $txt -notmatch "\[mark\] sweep$k at") { continue }
    $g = Get-Process gta_reversed -ErrorAction SilentlyContinue | Where-Object { $_.Path -and ($_.Path.Replace([string][char]92, '/') -like ($dir + '*')) } | Select-Object -First 1
    if (-not $g) { continue }
    if ($firstMark -eq $null) { $firstMark = Get-Date }
    $r = New-Object GrM+R; [GrM]::GetWindowRect($g.MainWindowHandle, [ref]$r) | Out-Null
    Check-User "before-sweep$k"
    [GrM]::SetForegroundWindow($g.MainWindowHandle) | Out-Null
    [GrM]::SetCursorPos([int](($r.L + $r.Rr) / 2), [int](($r.T + $r.B) / 2)) | Out-Null
    Start-Sleep -Milliseconds 300
    $own = [GrM]::Last()   # our own SetCursorPos / focus change above may have bumped the tick
    for ($i = 0; $i -lt $n; $i++) {
      [GrM]::mouse_event(1, $dx, 0, 0, [UIntPtr]::Zero); $own = [GrM]::Last()
      Start-Sleep -Milliseconds $delayMs
      Check-User "sweep$k.$i"
      if ([GrM]::GetForegroundWindow() -ne $g.MainWindowHandle) { [void]$foreign.Add([pscustomobject]@{ at = (Get-Date); what = "focus-lost sweep$k.$i" }) }
    }
    $done[$k] = 1
    "sweep $k sent ($n x $dx px)"
  }
}
# only events from 3 s before the first sweep mark count (anything earlier happened while the game was still starting)
$rel = @($foreign | Where-Object { $firstMark -ne $null -and $_.at -ge $firstMark.AddSeconds(-3) })
foreach ($f in ($rel | Select-Object -First 8)) { "USER_ACTIVE $($f.what) +$([int]($f.at - $firstMark).TotalSeconds)s" }
"SWEEPS_DONE sweeps=$($done.Count) user_active=$([int]($rel.Count -gt 0)) events=$($rel.Count)"
