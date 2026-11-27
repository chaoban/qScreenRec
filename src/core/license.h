#pragma once

#include <QString>

// 授權管理（預留）
// 目前 1.x 版為完整版，所有功能開放。
// 日後上架 Microsoft Store 或串接綠界金流時，只要新增 LicenseProvider 的實作
// （例如 StoreLicenseProvider 透過 Windows.Services.Store 查詢、
//  或 EcpayLicenseProvider 驗證序號），並在 License::provider() 切換即可，
// 其他程式碼只透過 License::hasFeature() 判斷功能，不需修改。
enum class Feature {
    Recording,
    Watermark,
    HighResolution,   // 1440p 以上
    LongRecording,    // 超過 10 分鐘
};

class LicenseProvider
{
public:
    virtual ~LicenseProvider() = default;
    virtual QString editionName() const = 0;
    virtual bool hasFeature(Feature f) const = 0;
};

class License
{
public:
    static const LicenseProvider &provider();
    static bool hasFeature(Feature f) { return provider().hasFeature(f); }
    static QString editionName() { return provider().editionName(); }
};
