# SOURCE this file (after hostenv.sh): helpers to kill ONLY processes the calling script started (agent rules v5 item 1: never kill by image name -
# `taskkill //IM`, `pkill -f gta_reversed`, `Stop-Process -Name` also kill other agents' runs / builds / the user's game).
#   own_kill_pid <bash-pid>          kill the job `cmd &` the script started ($!) and its whole Windows process tree (taskkill /T on its Windows pid, /proc/<pid>/winpid);
#                                    does nothing when that pid is already gone (no pid-reuse hazard: the winpid is read only while the bash pid is alive)
#   own_kill_in_dir <image> <dir>    Windows: kill the processes named <image> (no .exe) whose executable lies under <dir> - a PRIVATE run dir (the dir of the
#                                    gpu.lock holder / the soak data dir), so a stray child of an earlier attempt of THIS script dies, other copies of the same image never
own_kill_pid() {
  local p="${1:-}" w
  [ -n "$p" ] && kill -0 "$p" 2>/dev/null || return 0
  w=$(cat "/proc/$p/winpid" 2>/dev/null)
  [ -n "$w" ] && taskkill //F //T //PID "$w" >/dev/null 2>&1
  kill -9 "$p" 2>/dev/null
  return 0
}
own_kill_in_dir() {
  OWNK_NAME="$1" OWNK_DIR="$(hp "$2")" powershell.exe -NoProfile -Command 'Get-Process $env:OWNK_NAME -ErrorAction SilentlyContinue | Where-Object { $_.Path -and (($_.Path -replace "\\","/") -like ($env:OWNK_DIR + "/*")) } | Stop-Process -Force' >/dev/null 2>&1
  return 0
}
