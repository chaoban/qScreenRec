# 上架流程與檢查清單

## 一、前置作業（只需做一次）

- [ ] 註冊 Microsoft 開發人員帳戶：<https://partner.microsoft.com/dashboard/registration>（個人帳戶目前免費；公司帳戶需驗證）
- [ ] 在 Partner Center 建立新應用程式，保留名稱 **qScreenRec**
- [ ] 到「產品管理 → 產品身分識別」記下三個值：
  - `Package/Identity/Name`：【待填】
  - `Package/Identity/Publisher`：【待填，形如 CN=XXXXXXXX-XXXX-…】
  - `Package/Properties/PublisherDisplayName`：【待填】
- [ ] 把隱私權政策放到公開網址（GitHub Pages、Notion 公開頁面或個人網站皆可）

## 二、打包

```powershell
# 1. 取得 LGPL 版 FFmpeg（或把現有的 LGPL 版 ffmpeg.exe 放到 third_party\ffmpeg\）
powershell -ExecutionPolicy Bypass -File scripts\fetch_ffmpeg.ps1
# 2. 編譯並組成 dist\qScreenRec
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
# 3. 打包 MSIX（三個值換成 Partner Center 的產品身分識別）
powershell -ExecutionPolicy Bypass -File scripts\build_msix.ps1 `
    -Version 1.0.0.0 `
    -IdentityName "【Package/Identity/Name】" `
    -Publisher "【Package/Identity/Publisher】" `
    -PublisherDisplayName "【PublisherDisplayName】"
```

- [ ] 版本號第四碼必須是 0（例如 1.0.1.0），每次送審都要比上次大
- [ ] 本機測試：加 `-Sign` 產生自簽版本，安裝後確認可錄影、系統匣、快捷鍵、F1、中英文切換都正常
- [x] **Windows App Certification Kit**：2026-11-27 於開發機測試 1.0.0.0，整體結果 PASS（25 項必要測試通過；選用項目「Blocked executables」標示啟動 ffmpeg.exe，屬設計本身，不影響上架）

## 三、Partner Center 送審內容

- [ ] 定價與可用性：市場、價格（見 [pricing_and_monetization.md](pricing_and_monetization.md)）
- [ ] 屬性：類別「生產力」或「相片與影片」、隱私權網址、系統需求（見 [properties.md](properties.md)）
- [ ] 年齡分級：填 IARC 問卷（見 [age_rating.md](age_rating.md)）
- [ ] 套件：上傳 `dist\qScreenRec_1.0.0.0_x64.msix`（不需自行簽章）
- [ ] 商店清單：中文與英文各一份（見 listing 檔），至少 1 張截圖，建議 4～6 張
- [ ] 送審說明（Notes for certification）：

  > qScreenRec runs in the system tray. After launch, find the orange icon in the notification area (click "^" if hidden). Press Ctrl+Alt+R to start and stop recording; recordings are saved to Videos\qScreenRec. No account or network connection is needed. The app declares runFullTrust because it is a Win32 desktop app that captures the screen with DXGI Desktop Duplication and runs the bundled FFmpeg executable for encoding.

## 四、受限功能說明（runFullTrust）

送審時 Partner Center 會要求說明為何需要 `runFullTrust`，可填：

> qScreenRec is a Win32 (Qt) desktop application packaged with MSIX. It needs full trust to capture the desktop via DXGI Desktop Duplication, register a global hotkey, show a system tray icon, and launch the bundled FFmpeg encoder process.

## 五、授權合規

- [ ] 只附 **LGPL** 版 FFmpeg（`fetch_ffmpeg.ps1` 預設下載的就是 LGPL 版），不可用含 x264 的 GPL 版
- [ ] Qt 以 DLL 動態連結（windeployqt 已處理），關於頁與「授權資訊」已列出 Qt / FFmpeg 授權與原始碼網址
- [ ] 若有人來信索取 Qt / FFmpeg 原始碼，需提供對應版本
