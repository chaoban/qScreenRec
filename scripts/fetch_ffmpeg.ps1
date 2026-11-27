<#
.SYNOPSIS
  下載 LGPL 授權的 FFmpeg Windows 版（BtbN/FFmpeg-Builds）到 third_party\ffmpeg
.NOTES
  只使用 LGPL 版本（不含 x264 等 GPL 元件），才能搭配閉源商業軟體散布。
  也可直接把你已有的 LGPL 版 ffmpeg.exe 複製到 third_party\ffmpeg\。
#>
param(
    [string]$Url = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-lgpl.zip"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Out = Join-Path $Root "third_party\ffmpeg"
$Tmp = Join-Path $env:TEMP "qscreenrec_ffmpeg"
Remove-Item $Tmp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $Tmp, $Out | Out-Null

Write-Host "Downloading $Url"
Invoke-WebRequest $Url -OutFile "$Tmp\ffmpeg.zip" -UseBasicParsing
Expand-Archive "$Tmp\ffmpeg.zip" -DestinationPath $Tmp -Force
$bin = Get-ChildItem $Tmp -Recurse -Filter ffmpeg.exe | Select-Object -First 1
if (-not $bin) { throw "壓縮檔中找不到 ffmpeg.exe" }
Copy-Item $bin.FullName $Out -Force
$lic = Get-ChildItem $Tmp -Recurse -Filter "LICENSE*" | Select-Object -First 1
if ($lic) { Copy-Item $lic.FullName "$Out\LICENSE.txt" -Force }
& "$Out\ffmpeg.exe" -hide_banner -version | Select-Object -First 1
Write-Host "FFmpeg 已放到 $Out" -ForegroundColor Green
