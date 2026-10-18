$py = 'C:\Users\littl\AppData\Local\Programs\Python\Python312\python.exe'
$pids = @()
1..16 | ForEach-Object { $p = Start-Process -FilePath $py -ArgumentList 'C:\gta-scratch\skinflaky\busy.py' -WindowStyle Hidden -PassThru; $pids += $p.Id }
Start-Sleep 1
foreach ($id in $pids) { try { (Get-Process -Id $id).PriorityClass = "Idle" } catch {} }
($pids -join ',') | Set-Content C:\gta-scratch\skinflaky\stress.pids
