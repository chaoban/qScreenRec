#include "core/recorder.h"

#include "capture/capturer.h"
#include "capture/gdicapturer.h"
#include "core/ffmpeg.h"
#include "core/watermark.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cstring>
#include <windows.h>

namespace {

// Windows 命令列參數跳脫（CommandLineToArgvW 規則）
QString quoteArg(const QString &arg)
{
    if (!arg.isEmpty() && !arg.contains(QRegularExpression(QStringLiteral("[\\s\"]"))))
        return arg;
    QString out = QStringLiteral("\"");
    int backslashes = 0;
    for (QChar c : arg) {
        if (c == QLatin1Char('\\')) {
            ++backslashes;
        } else if (c == QLatin1Char('"')) {
            out += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            out += c;
            backslashes = 0;
        } else {
            out += QString(backslashes, QLatin1Char('\\'));
            out += c;
            backslashes = 0;
        }
    }
    out += QString(backslashes * 2, QLatin1Char('\\'));
    out += QLatin1Char('"');
    return out;
}

// 以 stdin 管線餵資料給 ffmpeg 的子行程
class PipeProcess
{
public:
    ~PipeProcess() { kill(); }

    bool start(const QString &exe, const QStringList &args, QString *error)
    {
        m_logPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                        .filePath(QStringLiteral("qScreenRec_ffmpeg.log"));

        SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
        HANDLE readEnd = nullptr;
        if (!CreatePipe(&readEnd, &m_stdin, &sa, 8 * 1024 * 1024)) {
            if (error) *error = QStringLiteral("CreatePipe failed");
            return false;
        }
        SetHandleInformation(m_stdin, HANDLE_FLAG_INHERIT, 0);

        HANDLE log = CreateFileW(reinterpret_cast<LPCWSTR>(m_logPath.utf16()), GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

        QString cmd = quoteArg(QDir::toNativeSeparators(exe));
        for (const QString &a : args)
            cmd += QLatin1Char(' ') + quoteArg(a);
        std::wstring cmdLine = cmd.toStdWString();

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = readEnd;
        si.hStdOutput = nul;
        si.hStdError = log != INVALID_HANDLE_VALUE ? log : nul;
        PROCESS_INFORMATION pi{};
        const BOOL ok = CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                                       CREATE_NO_WINDOW,
                                       nullptr, nullptr, &si, &pi);
        CloseHandle(readEnd);
        if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
        if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
        if (!ok) {
            CloseHandle(m_stdin);
            m_stdin = nullptr;
            if (error) *error = QStringLiteral("CreateProcess failed (%1): %2").arg(GetLastError()).arg(exe);
            return false;
        }
        CloseHandle(pi.hThread);
        m_process = pi.hProcess;
        return true;
    }

    bool write(const void *data, size_t size)
    {
        const auto *p = static_cast<const char *>(data);
        while (size > 0) {
            DWORD chunk = static_cast<DWORD>(qMin<size_t>(size, 64 * 1024 * 1024));
            DWORD written = 0;
            if (!WriteFile(m_stdin, p, chunk, &written, nullptr) || written == 0)
                return false;
            p += written;
            size -= written;
        }
        return true;
    }

    // 關閉 stdin 讓 ffmpeg 收尾，回傳 exit code（逾時回傳 -1）
    int finish(DWORD timeoutMs)
    {
        if (m_stdin) {
            CloseHandle(m_stdin);
            m_stdin = nullptr;
        }
        if (!m_process)
            return -1;
        if (WaitForSingleObject(m_process, timeoutMs) != WAIT_OBJECT_0) {
            kill();
            return -1;
        }
        DWORD code = 1;
        GetExitCodeProcess(m_process, &code);
        CloseHandle(m_process);
        m_process = nullptr;
        return static_cast<int>(code);
    }

    void kill()
    {
        if (m_stdin) {
            CloseHandle(m_stdin);
            m_stdin = nullptr;
        }
        if (m_process) {
            TerminateProcess(m_process, 1);
            WaitForSingleObject(m_process, 3000);
            CloseHandle(m_process);
            m_process = nullptr;
        }
    }

    QString logTail() const
    {
        QFile f(m_logPath);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        const QByteArray all = f.readAll();
        return QString::fromLocal8Bit(all.right(600)).trimmed();
    }

private:
    HANDLE m_stdin = nullptr;
    HANDLE m_process = nullptr;
    QString m_logPath;
};

// 合成用畫布：DIB section，同時可給 GDI（畫游標）與 QPainter（浮水印、點擊光圈）使用
class Canvas
{
public:
    ~Canvas()
    {
        if (m_dc) {
            SelectObject(m_dc, m_old);
            DeleteDC(m_dc);
        }
        if (m_bmp)
            DeleteObject(m_bmp);
    }

    bool create(const QSize &size)
    {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = size.width();
        bmi.bmiHeader.biHeight = -size.height();
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        m_dc = CreateCompatibleDC(nullptr);
        m_bmp = CreateDIBSection(m_dc, &bmi, DIB_RGB_COLORS, &m_bits, nullptr, 0);
        if (!m_dc || !m_bmp)
            return false;
        m_old = SelectObject(m_dc, m_bmp);
        m_image = QImage(static_cast<uchar *>(m_bits), size.width(), size.height(),
                         size.width() * 4, QImage::Format_RGB32);
        return true;
    }

    QImage &image() { return m_image; }
    const void *bits() const { return m_bits; }
    size_t byteCount() const { return static_cast<size_t>(m_image.width()) * m_image.height() * 4; }

    void copyFrom(const QImage &src)
    {
        if (src.size() == m_image.size() && src.bytesPerLine() == m_image.bytesPerLine()) {
            std::memcpy(m_bits, src.constBits(), byteCount());
        } else {
            QPainter p(&m_image);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.drawImage(m_image.rect(), src);
        }
    }

    // 回傳游標在畫布上的位置（指尖），不在畫面內回傳 false
    bool cursorInfo(const QRect &monitorRect, QPoint *tip, HCURSOR *cursor) const
    {
        CURSORINFO ci{};
        ci.cbSize = sizeof(ci);
        if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING) || !ci.hCursor)
            return false;
        const QPoint pt(ci.ptScreenPos.x - monitorRect.left(), ci.ptScreenPos.y - monitorRect.top());
        if (pt.x() < -64 || pt.y() < -64 || pt.x() > monitorRect.width() + 64 || pt.y() > monitorRect.height() + 64)
            return false;
        *tip = pt;
        *cursor = ci.hCursor;
        return true;
    }

    void drawCursor(HCURSOR cursor, const QPoint &tip)
    {
        QPoint hot = m_hotspots.value(cursor, QPoint(-1, -1));
        if (hot.x() < 0) {
            ICONINFO ii{};
            hot = QPoint(0, 0);
            if (GetIconInfo(cursor, &ii)) {
                hot = QPoint(static_cast<int>(ii.xHotspot), static_cast<int>(ii.yHotspot));
                if (ii.hbmMask) DeleteObject(ii.hbmMask);
                if (ii.hbmColor) DeleteObject(ii.hbmColor);
            }
            m_hotspots.insert(cursor, hot);
        }
        DrawIconEx(m_dc, tip.x() - hot.x(), tip.y() - hot.y(), cursor, 0, 0, 0, nullptr, DI_NORMAL);
        GdiFlush();
    }

private:
    HDC m_dc = nullptr;
    HBITMAP m_bmp = nullptr;
    HGDIOBJ m_old = nullptr;
    void *m_bits = nullptr;
    QImage m_image;
    QHash<HCURSOR, QPoint> m_hotspots;
};

void drawClickHighlight(QPainter &p, const QPoint &tip, int frameHeight)
{
    const bool left = (GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON) & 0x8000) != 0;
    const bool right = (GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_LBUTTON : VK_RBUTTON) & 0x8000) != 0;
    if (!left && !right)
        return;
    const int r = qMax(14, frameHeight / 45);
    const QColor c = left ? QColor(255, 200, 0, 110) : QColor(60, 160, 255, 110);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c.darker(130), 2));
    p.setBrush(c);
    p.drawEllipse(tip, r, r);
}

} // namespace

Recorder::Recorder(QObject *parent) : QObject(parent) {}

Recorder::~Recorder()
{
    m_stop = true;
    if (m_thread.joinable())
        m_thread.join();
}

QString Recorder::backendName() const
{
    switch (m_backend.load()) {
    case 1: return QStringLiteral("DXGI");
    case 2: return QStringLiteral("GDI");
    default: return QString();
    }
}

void Recorder::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

bool Recorder::start(const AppSettings &settings, const QString &ffmpegPath, const QString &encoderId, QString *error)
{
    if (m_state != State::Idle)
        return false;
    if (m_thread.joinable())
        m_thread.join();
    if (ffmpegPath.isEmpty()) {
        if (error) *error = tr("FFmpeg was not found. Please set its location in Settings.");
        return false;
    }
    QDir dir(settings.outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error) *error = tr("Cannot create the output folder:\n%1").arg(settings.outputDir);
        return false;
    }

    const QString name = QStringLiteral("qScreenRec_%1.%2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")),
                                  settings.fileExtension());
    m_file = QDir::toNativeSeparators(dir.filePath(name));
    m_stop = false;
    m_elapsedMs = 0;
    m_backend = 0;
    setState(State::Recording);
    m_thread = std::thread(&Recorder::run, this, Job{ settings, ffmpegPath, encoderId, m_file });
    return true;
}

void Recorder::stop()
{
    if (m_state == State::Recording) {
        m_stop = true;
        setState(State::Finishing);
    }
}

void Recorder::run(Job job)
{
    const AppSettings &s = job.settings;
    bool limitReached = false;
    QString error;

    auto finish = [&](bool ok) {
        QMetaObject::invokeMethod(this, [this, file = job.file, ok, limitReached, error]() {
            if (m_thread.joinable())
                m_thread.join();
            setState(State::Idle);
            emit finished(file, ok, limitReached, error);
        }, Qt::QueuedConnection);
    };

    // 1. 選擇螢幕並開始擷取
    const QList<MonitorInfo> monitors = Capturer::monitors();
    if (monitors.isEmpty()) {
        error = tr("No monitor was found.");
        finish(false);
        return;
    }
    const MonitorInfo monitor = monitors.value(qBound(0, s.monitorIndex, int(monitors.size()) - 1));
    std::unique_ptr<Capturer> cap = Capturer::create(monitor, &error);
    if (!cap) {
        error = tr("Screen capture could not be started.") + QLatin1Char('\n') + error;
        finish(false);
        return;
    }
    m_backend = cap->backendName() == QLatin1String("DXGI") ? 1 : 2;
    cap->acquire(250); // 取得第一張完整畫面

    const QSize frameSize = cap->image().size();
    Canvas canvas;
    if (frameSize.isEmpty() || !canvas.create(frameSize)) {
        error = tr("Failed to allocate the frame buffer.");
        finish(false);
        return;
    }

    Watermark watermark;
    if (s.watermarkEnabled)
        watermark.configure(s.watermarkText, s.watermarkPos, s.watermarkOpacity, frameSize);

    // 2. 啟動 ffmpeg
    const int fps = s.format == VideoFormat::Gif ? qMin(s.fps, 15) : s.fps;
    PipeProcess ff;
    if (!ff.start(job.ffmpegPath, FFmpeg::buildArgs(s, job.encoderId, frameSize, fps, job.file), &error)) {
        finish(false);
        return;
    }

    // 3. 主迴圈：固定影格率輸出；畫面沒變時重複上一格，確保影片長度與實際時間一致
    const double interval = 1000.0 / fps;
    const qint64 limitMs = s.durationLimitMin > 0 ? qint64(s.durationLimitMin) * 60 * 1000 : 0;
    qint64 written = 0;
    int failures = 0;
    bool writeFailed = false;
    QElapsedTimer timer;
    timer.start();

    while (!m_stop) {
        qint64 now = timer.elapsed();
        if (limitMs > 0 && now >= limitMs) {
            limitReached = true;
            break;
        }
        const double nextDue = written * interval;
        const int waitMs = qMax(0, static_cast<int>(nextDue - now));

        const Capturer::Status st = cap->acquire(qMin(waitMs, 100));
        if (st == Capturer::Status::Lost || st == Capturer::Status::Failed) {
            if (st == Capturer::Status::Failed)
                ++failures;
            QString reopenErr;
            if (st == Capturer::Status::Failed && failures > 30) {
                // DXGI 持續失敗 → 改用 GDI
                auto gdi = std::make_unique<GdiCapturer>();
                if (gdi->open(monitor, &reopenErr)) {
                    cap = std::move(gdi);
                    m_backend = 2;
                }
                failures = 0;
            } else if (st == Capturer::Status::Lost && !cap->open(monitor, &reopenErr)) {
                Sleep(50); // 例如 UAC 安全桌面期間，稍後再試；期間持續輸出上一格
            }
        } else {
            failures = 0;
        }

        now = timer.elapsed();
        if (now < nextDue)
            continue;

        canvas.copyFrom(cap->image());
        QPoint tip;
        HCURSOR cursor = nullptr;
        const bool hasCursor = s.drawCursor && canvas.cursorInfo(monitor.rect, &tip, &cursor);
        if (!watermark.isEmpty() || (hasCursor && s.highlightClicks)) {
            QPainter p(&canvas.image());
            if (hasCursor && s.highlightClicks)
                drawClickHighlight(p, tip, frameSize.height());
            watermark.paint(p);
        }
        if (hasCursor)
            canvas.drawCursor(cursor, tip);

        const qint64 due = static_cast<qint64>(now / interval) + 1;
        qint64 count = due - written;
        if (count > fps) { // 落後超過 1 秒（系統過載），放棄補格避免越拖越慢
            written = due - 1;
            count = 1;
        }
        for (qint64 i = 0; i < count; ++i) {
            if (!ff.write(canvas.bits(), canvas.byteCount())) {
                writeFailed = true;
                break;
            }
        }
        if (writeFailed)
            break;
        written += count;
        m_elapsedMs = now;
    }

    // 4. 收尾
    cap.reset();
    const int code = ff.finish(10 * 60 * 1000);
    const QFileInfo fi(job.file);
    const bool ok = code == 0 && !writeFailed && fi.exists() && fi.size() > 0;
    if (!ok) {
        error = tr("FFmpeg reported an error (exit code %1).").arg(code);
        const QString tail = ff.logTail();
        if (!tail.isEmpty())
            error += QStringLiteral("\n\n") + tail;
    }
    finish(ok);
}
