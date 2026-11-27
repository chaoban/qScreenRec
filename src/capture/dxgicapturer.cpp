#include "capture/dxgicapturer.h"

#include <cstring>

using Microsoft::WRL::ComPtr;

static QString hrText(const char *what, HRESULT hr)
{
    return QStringLiteral("DXGI: %1 failed (0x%2)")
        .arg(QString::fromLatin1(what))
        .arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
}

bool DxgiCapturer::open(const MonitorInfo &monitor, QString *error)
{
    m_monitor = monitor;
    m_dupl.Reset();
    m_staging.Reset();
    m_context.Reset();
    m_device.Reset();

    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        if (error) *error = hrText("CreateDXGIFactory1", hr);
        return false;
    }

    // 找出與指定螢幕（HMONITOR）對應的 adapter / output
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIOutput> output;
    for (UINT a = 0; !output && factory->EnumAdapters1(a, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++a) {
        ComPtr<IDXGIOutput> out;
        for (UINT o = 0; adapter->EnumOutputs(o, out.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++o) {
            DXGI_OUTPUT_DESC desc{};
            if (SUCCEEDED(out->GetDesc(&desc)) &&
                reinterpret_cast<quintptr>(desc.Monitor) == monitor.handle) {
                output = out;
                break;
            }
        }
    }
    if (!output) {
        if (error) *error = QStringLiteral("DXGI: monitor not found on any adapter");
        return false;
    }

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                           levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                           m_device.GetAddressOf(), nullptr, m_context.GetAddressOf());
    if (FAILED(hr)) {
        if (error) *error = hrText("D3D11CreateDevice", hr);
        return false;
    }

    ComPtr<IDXGIOutput1> output1;
    hr = output.As(&output1);
    if (FAILED(hr)) {
        if (error) *error = hrText("IDXGIOutput1", hr);
        return false;
    }
    hr = output1->DuplicateOutput(m_device.Get(), m_dupl.GetAddressOf());
    if (FAILED(hr)) {
        if (error) *error = hrText("DuplicateOutput", hr);
        return false;
    }

    DXGI_OUTDUPL_DESC dd{};
    m_dupl->GetDesc(&dd);
    const int w = static_cast<int>(dd.ModeDesc.Width);
    const int h = static_cast<int>(dd.ModeDesc.Height);
    if (m_image.width() != w || m_image.height() != h) {
        m_image = QImage(w, h, QImage::Format_RGB32);
        m_image.fill(Qt::black);
    }
    return true;
}

bool DxgiCapturer::copyToImage(ID3D11Texture2D *texture)
{
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);

    if (m_staging) {
        D3D11_TEXTURE2D_DESC sd{};
        m_staging->GetDesc(&sd);
        if (sd.Width != desc.Width || sd.Height != desc.Height || sd.Format != desc.Format)
            m_staging.Reset();
    }
    if (!m_staging) {
        D3D11_TEXTURE2D_DESC sd = desc;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.BindFlags = 0;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sd.MiscFlags = 0;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        if (FAILED(m_device->CreateTexture2D(&sd, nullptr, m_staging.GetAddressOf())))
            return false;
    }

    m_context->CopyResource(m_staging.Get(), texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_context->Map(m_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return false;

    const int w = static_cast<int>(desc.Width);
    const int h = static_cast<int>(desc.Height);
    if (m_image.width() != w || m_image.height() != h)
        m_image = QImage(w, h, QImage::Format_RGB32);
    const auto *src = static_cast<const uchar *>(mapped.pData);
    const size_t rowBytes = static_cast<size_t>(w) * 4;
    for (int y = 0; y < h; ++y)
        std::memcpy(m_image.scanLine(y), src + static_cast<size_t>(y) * mapped.RowPitch, rowBytes);
    m_context->Unmap(m_staging.Get(), 0);
    return true;
}

Capturer::Status DxgiCapturer::acquire(int timeoutMs)
{
    if (!m_dupl)
        return Status::Lost;

    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;
    HRESULT hr = m_dupl->AcquireNextFrame(static_cast<UINT>(qMax(0, timeoutMs)), &info, resource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        return Status::NoChange;
    if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
        m_dupl.Reset();
        return Status::Lost; // 解析度變更、全螢幕切換、UAC 等，需重新 open()
    }
    if (FAILED(hr))
        return Status::Failed;

    Status status = Status::NoChange;
    if (info.LastPresentTime.QuadPart != 0) {
        ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(resource.As(&tex)) && copyToImage(tex.Get()))
            status = Status::NewFrame;
    }
    m_dupl->ReleaseFrame();
    return status;
}
