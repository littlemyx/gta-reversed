$f = 'C:\gta-scratch\skinflaky\stress.pids'
if (Test-Path $f) {
  (Get-Content $f) -split ',' | Where-Object { $_ } | ForEach-Object { Stop-Process -Id ([int]$_) -Force -ErrorAction SilentlyContinue }
  Remove-Item $f
}
