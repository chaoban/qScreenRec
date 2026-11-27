<#
.SYNOPSIS
  把 dist\qScreenRec 打包成 MSIX（上傳 Microsoft Store 用）
.PARAMETER Publisher
  Partner Center →「產品身分識別」頁的 Package/Identity/Publisher，例如 CN=1A2B3C4D-....
.PARAMETER IdentityName
  Partner Center 的 Package/Identity/Name，例如 12345SAISAI.qScreenRec
.PARAMETER PublisherDisplayName
  Partner Center 的 PublisherDisplayName
.PARAMETER Sign
  本機測試用：以自簽憑證簽章（上傳 Store 不需簽章，Store 會重新簽）
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\build_msix.ps1 -Version 1.0.0.0
#>
param(
    [string]$Version = "1.0.0.0",
    [string]$IdentityName = "SAISAI.Dev.qScreenRec",
    [string]$Publisher = "CN=8EE2A838-6C91-4290-8711-F41153FB93C1",
    [string]$PublisherDisplayName = "SAISAI.Dev",
    # 必須是 Partner Center 已保留的應用程式名稱
    [string]$DisplayName = "qScreenRec 螢幕畫面錄製",
    [switch]$Sign
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Dist = Join-Path $Root "dist\qScreenRec"
if (-not (Test-Path "$Dist\qScreenRec.exe")) { throw "請先執行 scripts\build.ps1" }

# Windows SDK 工具
$kit = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.*\x64" -Directory |
    Sort-Object Name -Descending | Where-Object { Test-Path "$($_.FullName)\makeappx.exe" } | Select-Object -First 1
if (-not $kit) { throw "找不到 Windows 10/11 SDK（makeappx.exe），請在 Visual Studio Installer 安裝 Windows SDK" }
$makeappx = "$($kit.FullName)\makeappx.exe"
$makepri = "$($kit.FullName)\makepri.exe"
$signtool = "$($kit.FullName)\signtool.exe"

# 組 staging 資料夾
$Stage = Join-Path $Root "build\msix"
Remove-Item $Stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $Stage | Out-Null
Copy-Item "$Dist\*" $Stage -Recurse
Copy-Item "$Root\packaging\msix\Assets" $Stage -Recurse

$manifest = Get-Content "$Root\packaging\msix\AppxManifest.xml" -Raw -Encoding UTF8
$manifest = $manifest.Replace('{{IDENTITY_NAME}}', $IdentityName).Replace('{{PUBLISHER}}', $Publisher).
    Replace('{{PUBLISHER_DISPLAY_NAME}}', $PublisherDisplayName).Replace('{{VERSION}}', $Version).Replace('{{DISPLAY_NAME}}', $DisplayName)
[IO.File]::WriteAllText("$Stage\AppxManifest.xml", $manifest, (New-Object Text.UTF8Encoding($false)))

# 產生 resources.pri（讓 scale-100/200 圖示正確對應）
Push-Location $Stage
& $makepri createconfig /cf priconfig.xml /dq en-US_zh-TW /o | Out-Null
if ($LASTEXITCODE) { throw "makepri createconfig 失敗" }
# 單一 MSIX（非 bundle）不可拆分資源套件：預設 priconfig 的 autoResourcePackage 會把 scale-200
# 拆到 resources.scale-200.pri，主 resources.pri 就只剩 scale-100 候選，@2x 圖示永遠不會被用到
$priCfg = [IO.File]::ReadAllText("$Stage\priconfig.xml")
$priCfg = [regex]::Replace($priCfg, '(?s)\s*<packaging>.*?</packaging>', '')
[IO.File]::WriteAllText("$Stage\priconfig.xml", $priCfg, (New-Object Text.UTF8Encoding($false)))
& $makepri new /pr . /cf priconfig.xml /of resources.pri /o | Out-Null
if ($LASTEXITCODE) { throw "makepri new 失敗" }
Remove-Item priconfig.xml
Pop-Location

$OutDir = Join-Path $Root "dist"
$Msix = Join-Path $OutDir "qScreenRec_$Version`_x64.msix"
& $makeappx pack /d $Stage /p $Msix /o
if ($LASTEXITCODE) { throw "makeappx 失敗" }

if ($Sign) {
    # 本機測試：建立與 Publisher 相同主體的自簽憑證，並匯入「受信任的人」
    $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $Publisher } | Select-Object -First 1
    if (-not $cert) {
        $cert = New-SelfSignedCertificate -Type Custom -Subject $Publisher -KeyUsage DigitalSignature `
            -FriendlyName "qScreenRec test" -CertStoreLocation Cert:\CurrentUser\My `
            -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
    }
    $pfx = Join-Path $OutDir "qScreenRec_test.pfx"
    $pfxPass = ConvertTo-SecureString -String "qscreenrec" -Force -AsPlainText
    Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $pfxPass | Out-Null
    & $signtool sign /fd SHA256 /f $pfx /p qscreenrec $Msix
    Write-Host "測試安裝前，請以系統管理員把 $pfx 匯入「本機電腦 → 受信任的人」" -ForegroundColor Yellow
}
Write-Host "`n完成：$Msix" -ForegroundColor Green
