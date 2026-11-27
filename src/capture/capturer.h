#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QString>
#include <memory>

struct MonitorInfo
{
    int index = 0;
    QString deviceName;     // 例如 \\.\DISPLAY1
    QRect rect;             // 實體像素座標（Per-Monitor DPI aware）
    bool primary = false;
    quintptr handle = 0;    // HMONITOR

    QString displayName() const;
};

// 畫面擷取介面：DXGI Desktop Duplication 為主，GDI 為備援
class Capturer
{
public:
    enum class Status { NewFrame, NoChange, Lost, Failed };

    virtual ~Capturer() = default;
    virtual bool open(const MonitorInfo &monitor, QString *error) = 0;
    // 等待最多 timeoutMs 毫秒取得新畫面；畫面內容存在 image()
    virtual Status acquire(int timeoutMs) = 0;
    virtual const QImage &image() const = 0;
    virtual QString backendName() const = 0;

    const MonitorInfo &monitor() const { return m_monitor; }

    static QList<MonitorInfo> monitors();
    // 依序嘗試 DXGI → GDI
    static std::unique_ptr<Capturer> create(const MonitorInfo &monitor, QString *error);

protected:
    MonitorInfo m_monitor;
};
