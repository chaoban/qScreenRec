#include "capture/gdicapturer.h"

#include <cstring>
#include <windows.h>

GdiCapturer::~GdiCapturer()
{
    close();
}

void GdiCapturer::close()
{
    if (m_memDc) {
        if (m_oldBitmap)
            SelectObject(static_cast<HDC>(m_memDc), static_cast<HGDIOBJ>(m_oldBitmap));
        DeleteDC(static_cast<HDC>(m_memDc));
    }
    if (m_bitmap)
        DeleteObject(static_cast<HBITMAP>(m_bitmap));
    if (m_screenDc)
        ReleaseDC(nullptr, static_cast<HDC>(m_screenDc));
    m_memDc = m_bitmap = m_oldBitmap = m_screenDc = nullptr;
    m_bits = nullptr;
}

bool GdiCapturer::open(const MonitorInfo &monitor, QString *error)
{
    close();
    m_monitor = monitor;
    const int w = monitor.rect.width();
    const int h = monitor.rect.height();

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!screen || !mem || !bmp) {
        if (bmp) DeleteObject(bmp);
        if (mem) DeleteDC(mem);
        if (screen) ReleaseDC(nullptr, screen);
        if (error)
            *error = QStringLiteral("GDI: CreateDIBSection failed");
        return false;
    }
    m_screenDc = screen;
    m_memDc = mem;
    m_bitmap = bmp;
    m_oldBitmap = SelectObject(mem, bmp);
    m_bits = static_cast<uchar *>(bits);
    m_image = QImage(w, h, QImage::Format_RGB32);
    m_image.fill(Qt::black);
    return true;
}

Capturer::Status GdiCapturer::acquire(int timeoutMs)
{
    if (!m_memDc)
        return Status::Failed;
    if (timeoutMs > 0)
        Sleep(static_cast<DWORD>(timeoutMs));
    const QRect r = m_monitor.rect;
    if (!BitBlt(static_cast<HDC>(m_memDc), 0, 0, r.width(), r.height(),
                static_cast<HDC>(m_screenDc), r.left(), r.top(), SRCCOPY)) {
        return Status::NoChange; // 例如切換到安全桌面（UAC）時，保留上一張畫面
    }
    GdiFlush();
    const int stride = r.width() * 4;
    for (int y = 0; y < r.height(); ++y)
        std::memcpy(m_image.scanLine(y), m_bits + static_cast<size_t>(y) * stride, stride);
    return Status::NewFrame;
}
