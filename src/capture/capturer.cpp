#include "capture/capturer.h"
#include "capture/dxgicapturer.h"
#include "capture/gdicapturer.h"

#include <QCoreApplication>
#include <algorithm>
#include <windows.h>

QString MonitorInfo::displayName() const
{
    QString s = QCoreApplication::translate("Capturer", "Monitor %1 (%2×%3)")
                    .arg(index + 1)
                    .arg(rect.width())
                    .arg(rect.height());
    if (primary)
        s += QCoreApplication::translate("Capturer", " – Primary");
    return s;
}

static BOOL CALLBACK enumMonitorProc(HMONITOR hMon, HDC, LPRECT, LPARAM data)
{
    auto *list = reinterpret_cast<QList<MonitorInfo> *>(data);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi)) {
        MonitorInfo info;
        info.deviceName = QString::fromWCharArray(mi.szDevice);
        info.rect = QRect(mi.rcMonitor.left, mi.rcMonitor.top,
                          mi.rcMonitor.right - mi.rcMonitor.left,
                          mi.rcMonitor.bottom - mi.rcMonitor.top);
        info.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        info.handle = reinterpret_cast<quintptr>(hMon);
        list->append(info);
    }
    return TRUE;
}

QList<MonitorInfo> Capturer::monitors()
{
    QList<MonitorInfo> list;
    EnumDisplayMonitors(nullptr, nullptr, enumMonitorProc, reinterpret_cast<LPARAM>(&list));
    // 主螢幕排第一，其餘依位置由左至右
    std::stable_sort(list.begin(), list.end(), [](const MonitorInfo &a, const MonitorInfo &b) {
        if (a.primary != b.primary)
            return a.primary;
        if (a.rect.left() != b.rect.left())
            return a.rect.left() < b.rect.left();
        return a.rect.top() < b.rect.top();
    });
    for (int i = 0; i < list.size(); ++i)
        list[i].index = i;
    return list;
}

std::unique_ptr<Capturer> Capturer::create(const MonitorInfo &monitor, QString *error)
{
    QString dxgiError;
    auto dxgi = std::make_unique<DxgiCapturer>();
    if (dxgi->open(monitor, &dxgiError))
        return dxgi;

    auto gdi = std::make_unique<GdiCapturer>();
    QString gdiError;
    if (gdi->open(monitor, &gdiError))
        return gdi;

    if (error)
        *error = dxgiError + QLatin1Char('\n') + gdiError;
    return nullptr;
}
