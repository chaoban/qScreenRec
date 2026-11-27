<#
.SYNOPSIS
  編譯 qScreenRec 並輸出可攜版到 dist\qScreenRec
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\build.ps1
  powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64
#>
param(
    [string]$QtDir = "",
    [string]$Config = "Release"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

# 1. 找 Qt（預設搜尋 C:\Qt\6.*\msvc20*_64，取最新版）
if (-not $QtDir) {
    $cand = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^6\.\d+' } |
        Sort-Object { [version]($_.Name -replace '[^\d\.]', '') } -Descending |
        ForEach-Object { Get-ChildItem $_.FullName -Directory | Where-Object { $_.Name -match '^msvc20\d\d_64$' } } |
        Select-Object -First 1
    if (-not $cand) { throw "找不到 Qt6 MSVC 版本，請用 -QtDir 指定，例如 C:\Qt\6.8.3\msvc2022_64" }
    $QtDir = $cand.FullName
}
Write-Host "Qt: $QtDir" -ForegroundColor Cyan

# 2. 找 Visual Studio
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "找不到含 C++ 工具的 Visual Studio" }
Write-Host "Visual Studio: $vsPath" -ForegroundColor Cyan

# 3. CMake 設定與編譯（優先使用 VS 內附的 cmake）
$cmake = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = "cmake" }
& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$QtDir"
if ($LASTEXITCODE) { throw "CMake configure 失敗" }
& $cmake --build build --config $Config --parallel
if ($LASTEXITCODE) { throw "編譯失敗" }

# 4. 組成 dist\qScreenRec
$Dist = Join-Path $Root "dist\qScreenRec"
if (Test-Path $Dist) { Remove-Item $Dist -Recurse -Force }
New-Item -ItemType Directory -Force $Dist | Out-Null
Copy-Item "build\$Config\qScreenRec.exe" $Dist

& "$QtDir\bin\windeployqt.exe" --release --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler `
    --no-network --translations zh_TW "$Dist\qScreenRec.exe"
if ($LASTEXITCODE) { throw "windeployqt 失敗" }

# VC++ 執行階段（app-local，MSIX 也需要）
$crt = Get-ChildItem "$vsPath\VC\Redist\MSVC\*\x64\Microsoft.VC14*.CRT" -Directory -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
if ($crt) {
    Copy-Item "$($crt.FullName)\msvcp140*.dll", "$($crt.FullName)\vcruntime140*.dll" $Dist
} else {
    Write-Warning "找不到 VC++ Redist DLL，目標電腦需自行安裝 VC++ 2015-2022 x64 執行階段"
}

# FFmpeg（先執行 scripts\fetch_ffmpeg.ps1，或自行放 third_party\ffmpeg\ffmpeg.exe）
$ff = Join-Path $Root "third_party\ffmpeg\ffmpeg.exe"
if (Test-Path $ff) {
    # 授權檢查：拒絕打包 GPL / nonfree 版 FFmpeg（閉源商業軟體只能附 LGPL 版）
    $cfg = (& $ff -hide_banner -version) -join " "
    if ($cfg -match '--enable-gpl' -or $cfg -match '--enable-nonfree') {
        throw "third_party\ffmpeg\ffmpeg.exe 是 GPL 或 nonfree 版本，不能隨本軟體散布。請改用 LGPL 版（scripts\fetch_ffmpeg.ps1）"
    }
    New-Item -ItemType Directory -Force "$Dist\ffmpeg" | Out-Null
    Copy-Item $ff "$Dist\ffmpeg\"
    Get-ChildItem (Split-Path $ff) -Filter "LICENSE*" | Copy-Item -Destination "$Dist\ffmpeg\"
    # shared 版 FFmpeg（例如 BtbN lgpl-shared）需要同目錄的 av*.dll / sw*.dll
    Get-ChildItem (Split-Path $ff) -Filter "*.dll" | Copy-Item -Destination "$Dist\ffmpeg\"
    Get-ChildItem (Split-Path $ff) -Filter "SOURCE*" | Copy-Item -Destination "$Dist\ffmpeg\"
} else {
    Write-Warning "third_party\ffmpeg\ffmpeg.exe 不存在，dist 內不含 FFmpeg（請執行 scripts\fetch_ffmpeg.ps1）"
}

Write-Host "`n完成：$Dist\qScreenRec.exe" -ForegroundColor Green
