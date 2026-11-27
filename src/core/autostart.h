#pragma once

// 開機自動啟動（HKCU\...\Run）。MSIX 封裝版改用 manifest 的 StartupTask。
namespace AutoStart {
bool isEnabled();
bool setEnabled(bool enabled);
// 是否以 MSIX 封裝執行（Microsoft Store 版）
bool isPackaged();
}
