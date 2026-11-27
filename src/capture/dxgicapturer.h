#pragma once

#include "capture/capturer.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

// DXGI Desktop Duplication：效能最佳，Windows 8 以上支援
class DxgiCapturer : public Capturer
{
public:
    bool open(const MonitorInfo &monitor, QString *error) override;
    Status acquire(int timeoutMs) override;
    const QImage &image() const override { return m_image; }
    QString backendName() const override { return QStringLiteral("DXGI"); }

private:
    bool copyToImage(ID3D11Texture2D *texture);

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_dupl;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_staging;
    QImage m_image;
};
