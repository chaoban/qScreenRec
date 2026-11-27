# Microsoft Store 上架文件範本

這個資料夾放的是上架 Microsoft Store 時，Partner Center 需要填寫或上傳的內容範本。標示 `【待填】` 的地方請依實際情況修改。

| 檔案 | 用途 | 對應 Partner Center 位置 |
|---|---|---|
| [submission_checklist.md](submission_checklist.md) | 上架步驟總覽與檢查清單 | 全流程 |
| [compliance_risk_assessment.md](compliance_risk_assessment.md) | 授權、專利、Store 政策、穩定性與路徑的風險評估 | 送審前自我檢查 |
| [listing_zh-TW.md](listing_zh-TW.md) | 繁體中文商店頁內容 | 商店清單 → 中文（台灣） |
| [listing_en-US.md](listing_en-US.md) | 英文商店頁內容 | 商店清單 → English (United States) |
| [privacy_policy_zh-TW.md](privacy_policy_zh-TW.md) / [privacy_policy_en-US.md](privacy_policy_en-US.md) | 隱私權政策（需公開網址） | 屬性 → 隱私權原則 URL |
| [age_rating.md](age_rating.md) | IARC 年齡分級問卷建議答案 | 年齡分級 |
| [properties.md](properties.md) | 類別、系統需求、功能宣告 | 屬性 |
| [pricing_and_monetization.md](pricing_and_monetization.md) | 定價、試用、日後收費方式規劃 | 定價與可用性 |
| [screenshots.md](screenshots.md) | 截圖與宣傳圖規格與拍攝清單 | 商店清單 → 螢幕擷取畫面 |

打包程式：`scripts\build.ps1` → `scripts\build_msix.ps1`，詳見 [submission_checklist.md](submission_checklist.md)。
