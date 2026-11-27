#pragma once

#include "capture/capturer.h"

// 以 GDI BitBlt 擷取畫面；DXGI 無法使用時（如遠端桌面、部分雙顯卡筆電）的備援方案
class GdiCapturer : public Capturer
{
public:
    ~GdiCapturer() override;
    bool open(const MonitorInfo &monitor, QString *error) override;
    Status acquire(int timeoutMs) override;
    const QImage &image() const override { return m_image; }
    QString backendName() const override { return QStringLiteral("GDI"); }

private:
    void close();

    void *m_screenDc = nullptr;   // HDC
    void *m_memDc = nullptr;      // HDC
    void *m_bitmap = nullptr;     // HBITMAP
    void *m_oldBitmap = nullptr;  // HGDIOBJ
    uchar *m_bits = nullptr;
    QImage m_image;
};
