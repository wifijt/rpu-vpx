# rpu-vpx: put an RPU game's .hex into VPX (Windows). Copies it into the rpu folder beside VPinMAME64.dll with its
# settings and WAVs, and makes a copy of a table that runs it.
#   powershell -ExecutionPolicy Bypass -File tools\add-game.ps1 <sketch>.ino.hex <name> <machine> [-Rev n]
#       [-Sounds <folder>] [-Table <table>.vpx] [-RpuDir <folder>] [-Vpx <VPinballX64.exe>]
#   <name>     your game's name in PinMAME: rpu_<name> (letters, digits, _)
#   <machine>  PinMAME's name for the machine: scrpn_l1 (System 6), startrek or startreb (Bally -35), ...
#   -Rev       your build's RPU_OS_HARDWARE_REV; the default is 102 on System 6, 4 on Bally
#   -Sounds    your WAV Trigger files (001_name.wav, ...): copied to rpu\<name>\
#   -Table     a VPX table for that machine: copied beside it as "<table> (rpu_<name>).vpx", set to run your game
#   -RpuDir    the rpu folder, if VPinMAME64.dll isn't registered (Setup64.exe registers it)
#   -Vpx       VPinballX64.exe, to export the script of a table that has no .vbs beside it
param(
  [Parameter(Mandatory, Position = 0)][string]$Hex,
  [Parameter(Mandatory, Position = 1)][string]$Name,
  [Parameter(Mandatory, Position = 2)][string]$Machine,
  [int]$Rev = 0,
  [string]$Sounds,
  [string]$Table,
  [string]$RpuDir,
  [string]$Vpx
)
$ErrorActionPreference = 'Stop'
function Fail($msg) { Write-Host $msg -ForegroundColor Red; exit 1 }

if (-not (Test-Path -LiteralPath $Hex -PathType Leaf)) { Fail "no file $Hex" }
if ($Hex -match 'with_bootloader') { Fail 'use <sketch>.ino.hex, not the with_bootloader one' }
if ($Name -notmatch '^[A-Za-z0-9_]+$') { Fail 'the name takes letters, digits and _ only' }

# the rpu folder: beside the VPinMAME DLL that's registered
if (-not $RpuDir) {
  $clsid = (Get-ItemProperty -LiteralPath 'Registry::HKEY_CLASSES_ROOT\VPinMAME.Controller\CLSID' -ErrorAction SilentlyContinue).'(default)'
  $dll = if ($clsid) { (Get-ItemProperty -LiteralPath "Registry::HKEY_CLASSES_ROOT\CLSID\$clsid\InprocServer32" -ErrorAction SilentlyContinue).'(default)' }
  if (-not $dll) { Fail "VPinMAME isn't registered: run Setup64.exe from this kit, or give -RpuDir" }
  $RpuDir = Join-Path (Split-Path $dll) 'rpu'
}
New-Item -ItemType Directory -Force -Path $RpuDir | Out-Null
if (-not (Test-Path (Join-Path $RpuDir 'rpu_avr.dll'))) { Write-Host "note: $RpuDir\rpu_avr.dll isn't there: copy this kit's rpu folder beside VPinMAME64.dll" }

Copy-Item -LiteralPath $Hex -Destination (Join-Path $RpuDir "rpu_$Name.hex") -Force
$settings = @("rom $Machine")
if ($Rev) { $settings += "rev $Rev" }
Set-Content -LiteralPath (Join-Path $RpuDir "rpu_$Name.txt") -Value $settings -Encoding Ascii
Write-Host "game:   $RpuDir\rpu_$Name.hex, rpu_$Name.txt ($($settings -join ', '))"

if ($Sounds) {
  if (-not (Test-Path -LiteralPath $Sounds -PathType Container)) { Fail "no folder $Sounds" }
  $to = Join-Path $RpuDir $Name
  New-Item -ItemType Directory -Force -Path $to | Out-Null
  Copy-Item -Path (Join-Path $Sounds '*.wav') -Destination $to -Force
  Write-Host "sounds: $((Get-ChildItem -LiteralPath $to -Filter *.wav).Count) WAVs in $to"
}

if ($Table) {
  if (-not (Test-Path -LiteralPath $Table -PathType Leaf)) { Fail "no table $Table" }
  $t = Get-Item -LiteralPath $Table
  $base = Join-Path $t.DirectoryName $t.BaseName
  $new = "$base (rpu_$Name)"
  Copy-Item -LiteralPath $t.FullName -Destination "$new.vpx" -Force
  # the backglass: the table's own name, else the folder's only one
  $b2s = @(Get-ChildItem -LiteralPath $t.DirectoryName -Filter *.directb2s | Where-Object { $_.BaseName -eq $t.BaseName })
  if (-not $b2s) { $b2s = @(Get-ChildItem -LiteralPath $t.DirectoryName -Filter *.directb2s | Where-Object { $_.BaseName -notmatch '\(rpu_' }) }
  if ($b2s.Count -eq 1) { Copy-Item -LiteralPath $b2s[0].FullName -Destination "$new.directb2s" -Force; Write-Host "backglass: $new.directb2s" }
  if (Test-Path -LiteralPath "$base.vbs") { Copy-Item -LiteralPath "$base.vbs" -Destination "$new.vbs" -Force }
  else {                                                # the script inside the table: VPX exports it
    if (-not $Vpx) {
      $Vpx = @("$env:ProgramFiles\Visual Pinball\VPinballX64.exe", 'C:\vPinball\VisualPinball\VPinballX64.exe',
               (Join-Path (Split-Path $RpuDir -Parent | Split-Path -Parent) 'VisualPinball\VPinballX64.exe')) |
             Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if (-not $Vpx) { Fail "can't find VPinballX64.exe to export the table's script: give -Vpx" }
    Start-Process -FilePath $Vpx -ArgumentList '-ExtractVBS', "`"$new.vpx`"" -Wait
    if (-not (Test-Path -LiteralPath "$new.vbs")) { Fail "VPX didn't export the script of $Table" }
  }
  # the first cGameName = "..." is the table's game; every other byte and line ending kept
  $bytes = [Text.Encoding]::GetEncoding(28591)
  $vbs = [IO.File]::ReadAllText("$new.vbs", $bytes)
  $re = [regex]'(?i)(cGameName\s*=\s*")[^"]*"'
  if (-not $re.IsMatch($vbs)) { Fail 'no cGameName in the script' }
  [IO.File]::WriteAllText("$new.vbs", $re.Replace($vbs, "`${1}rpu_$Name`"", 1), $bytes)
  Write-Host "table:  $new.vpx (cGameName rpu_$Name)"
}
