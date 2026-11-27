# qScreenRec

常駐 Windows 系統匣的螢幕錄影工具。按一下快捷鍵開始錄影、再按一次停止，連同滑鼠游標一起錄下來。

A system-tray screen recorder for Windows 11. Press a hotkey to start, press it again to stop.

![icon](resources/icons/app.png)

## 功能

- 系統匣常駐、全域快捷鍵（預設 `Ctrl+Shift+R`，可自訂）開始／停止
- 錄下畫面與滑鼠游標，可標示滑鼠點擊
- 輸出格式：MP4、MKV（H.264）、GIF
- 解析度：原始、2160p、1440p、1080p、720p、480p
- 影格率 10～60 fps、畫質高／中／低
- 錄影時間上限（時間到自動存檔）、錄影前倒數
- 文字浮水印（20 字內，可選位置、透明度，含即時預覽）
- GPU 硬體編碼自動偵測（NVENC／Quick Sync／AMF／Media Foundation）
- 多螢幕選擇、開機自動啟動
- 繁體中文／英文介面，`F1` 開啟使用說明
- 關於頁（SAISAI Logo、版權、Qt／FFmpeg 授權資訊）

## 架構

| 模組 | 說明 |
|---|---|
| `src/capture/` | DXGI Desktop Duplication 擷取（失敗時自動改用 GDI） |
| `src/core/recorder.*` | 背景執行緒：擷取 → 合成游標／浮水印 → 以管線送入 `ffmpeg.exe` 編碼；固定影格率輸出 |
| `src/core/ffmpeg.*` | 尋找 ffmpeg、實測可用編碼器、組參數 |
| `src/core/hotkey.*` | Win32 `RegisterHotKey` 全域快捷鍵 |
| `src/core/license.*` | 授權介面（預留 Store／綠界收費） |
| `src/ui/` | 設定、關於、說明視窗與深色主題 |
| `i18n/` | 繁體中文翻譯（原文為英文） |
| `packaging/msix/` | MSIX manifest 與圖示 |
| `store/` | Microsoft Store 上架文件範本 |

## 編譯

需求：Windows 10/11 x64、Visual Studio 2022（含「使用 C++ 的桌面開發」與 Windows SDK）、Qt 6.4 以上 MSVC 64-bit 版本。

```powershell
# 1. 取得 LGPL 版 FFmpeg（或把現有 LGPL 版 ffmpeg.exe 放到 third_party\ffmpeg\）
powershell -ExecutionPolicy Bypass -File scripts\fetch_ffmpeg.ps1
# 2. 編譯，輸出到 dist\qScreenRec\
powershell -ExecutionPolicy Bypass -File scripts\build.ps1            # 自動尋找 C:\Qt\6.x\msvc20xx_64
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64
# 3. 打包 MSIX（上架用）
powershell -ExecutionPolicy Bypass -File scripts\build_msix.ps1 -Version 1.0.0.0
```

也可以直接用 Qt Creator 或 Visual Studio 開啟 `CMakeLists.txt`。執行時 `ffmpeg.exe` 需放在 `qScreenRec.exe` 旁、`ffmpeg\` 子資料夾，或系統 PATH 中，也可以在設定中指定。

### 修改翻譯

```powershell
lupdate src -ts i18n\qscreenrec_zh_TW.ts     # 更新字串
python scripts\fill_zh_tw.py                 # 套用 scripts 內的對照表（或用 Qt Linguist 編輯）
```

### 重新產生圖示

```powershell
pip install pillow
python scripts\make_icons.py
```

## 授權

© 2026 SAISAI.Dev 版權所有。本軟體使用 Qt（LGPL v3）與 FFmpeg（LGPL v2.1+），詳見 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
