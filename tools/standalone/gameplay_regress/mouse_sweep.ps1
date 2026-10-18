# usage: powershell -File mouse_sweep.ps1 <standalone.log> <exe-dir-prefix> [dx=100] [n=40] [delayMs=20] [sweeps=3]
# Side-car of the "input_mouse" scenario: for every `[mark] sweepK` that appears in the game's log, centre the real cursor on the game window and send n raw relative mouse moves of dx pixels (mouse_event).
param($log, $dir, $dx = 100, $n = 40, $delayMs = 20, $sweeps = 3, $maxWaitS = 240)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class GrM { [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
[DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y); [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r); [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
[StructLayout(LayoutKind.Sequential)] public struct R { public int L,T,Rr,B; } }
"@
[GrM]::SetProcessDPIAware() | Out-Null
$done = @{}
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt $maxWaitS -and $done.Count -lt $sweeps) {
  Start-Sleep -Milliseconds 200
  $txt = ''
  try { $fs = [System.IO.File]::Open($log, 'Open', 'Read', 'ReadWrite'); $sr = New-Object System.IO.StreamReader($fs); $txt = $sr.ReadToEnd(); $sr.Close(); $fs.Close() } catch { continue }
  for ($k = 1; $k -le $sweeps; $k++) {
    if ($done.ContainsKey($k) -or $txt -notmatch "\[mark\] sweep$k at") { continue }
    $g = Get-Process gta_reversed -ErrorAction SilentlyContinue | Where-Object { $_.Path -and ($_.Path.Replace([string][char]92, '/') -like ($dir + '*')) } | Select-Object -First 1
    if (-not $g) { continue }
    $r = New-Object GrM+R; [GrM]::GetWindowRect($g.MainWindowHandle, [ref]$r) | Out-Null
    [GrM]::SetForegroundWindow($g.MainWindowHandle) | Out-Null
    [GrM]::SetCursorPos([int](($r.L + $r.Rr) / 2), [int](($r.T + $r.B) / 2)) | Out-Null
    Start-Sleep -Milliseconds 300
    for ($i = 0; $i -lt $n; $i++) { [GrM]::mouse_event(1, $dx, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds $delayMs }
    $done[$k] = 1
    "sweep $k sent ($n x $dx px)"
  }
}
