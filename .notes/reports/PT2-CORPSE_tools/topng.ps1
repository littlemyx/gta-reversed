param([string]$Dir, [int]$MaxW = 960)
Add-Type -AssemblyName System.Drawing
Get-ChildItem -Path $Dir -Filter *.bmp | ForEach-Object {
  $png = [System.IO.Path]::ChangeExtension($_.FullName, '.png')
  if (Test-Path $png) { return }
  $img = [System.Drawing.Image]::FromFile($_.FullName)
  $w = [Math]::Min($img.Width, $MaxW); $h = [int]($img.Height * $w / $img.Width)
  $bmp = New-Object System.Drawing.Bitmap($w, $h)
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.InterpolationMode = 'HighQualityBicubic'
  $g.DrawImage($img, 0, 0, $w, $h); $g.Dispose()
  $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose(); $img.Dispose()
}
