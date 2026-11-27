# 定價與日後收費規劃

## v1（目前）
- 建議先以 **免費** 上架累積評價，或直接訂價（例如 NT$ 90～300）並開啟 **免費試用 7 天**（Store 內建，不需改程式）。
- 程式已在 `src/core/license.h` 預留授權介面，目前固定為「完整版」，所有功能開放。

## 日後可選的收費方式

| 方式 | 需要的程式修改 | 備註 |
|---|---|---|
| Store 付費下載 | 無 | 最簡單；Store 抽成依 Microsoft 當時規定 |
| Store 免費試用 → 付費 | 新增 `StoreLicenseProvider`，以 `Windows.Services.Store.StoreContext::GetAppLicenseAsync()` 判斷試用／正式 | 試用期間可限制錄影時間或加上浮水印 |
| Store 應用程式內購買（IAP / 附加元件） | 同上，再用 `RequestPurchaseAsync()` 購買附加元件 | 例如「進階版」解鎖 4K、無時間限制 |
| 綠界 ECPay（Store 以外的通路） | 新增 `EcpayLicenseProvider`：使用者在網站付款後取得序號，程式驗證序號 | 需自架簡易授權伺服器或離線序號簽章；Store 版是否可用第三方金流請依當時 Microsoft Store 政策確認 |

`License::hasFeature(Feature::...)` 已在介面上定義 `Watermark`、`HighResolution`、`LongRecording` 等功能代碼，未來只要在對應位置加判斷即可分級。
