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
#include <QStorageInfo>
#include <QProcess>
#include "capture/dxgicapturer.h"
#include <cstring>
#include <windows.h>
#include <timeapi.h>

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

    // 擷取到新畫面時更新桌面緩衝（畫面沒變就沿用上一張）
    void updateDesktop(const QImage &src)
    {
        if (m_desktop.size() != m_image.size())
            m_desktop = QImage(m_image.size(), QImage::Format_RGB32);
        if (src.size() == m_desktop.size() && src.bytesPerLine() == m_desktop.bytesPerLine()) {
            std::memcpy(m_desktop.bits(), src.constBits(), byteCount());
        } else if (!src.isNull()) {
            // 錄影中途解析度改變：縮放到原本的輸出大小，避免 ffmpeg 收到大小不符的畫面
            QPainter p(&m_desktop);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.fillRect(m_desktop.rect(), Qt::black);
            const QSize fit = src.size().scaled(m_desktop.size(), Qt::KeepAspectRatio);
            p.drawImage(QRect(QPoint((m_desktop.width() - fit.width()) / 2, (m_desktop.height() - fit.height()) / 2), fit), src);
        }
    }

    // 把桌面緩衝複製到輸出畫布，之後再疊加游標與浮水印
    void beginFrame()
    {
        if (m_desktop.isNull()) {
            std::memset(m_bits, 0, byteCount());
            return;
        }
        std::memcpy(m_bits, m_desktop.constBits(), byteCount());
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
    QImage m_desktop;
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

namespace {
// 檔名重複時加上 _2、_3…
QString uniqueFile(const QDir &dir, const QString &base, const QString &ext)
{
    QString name = QStringLiteral("%1.%2").arg(base, ext);
    for (int i = 2; QFileInfo::exists(dir.filePath(name)) && i < 1000; ++i)
        name = QStringLiteral("%1_%2.%3").arg(base).arg(i).arg(ext);
    return QDir::toNativeSeparators(dir.filePath(name));
}

bool ensureWritableDir(const QString &path)
{
    if (path.isEmpty())
        return false;
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;
    // 實際寫入測試（唯讀磁碟、權限不足、拔除的隨身碟）
    QFile probe(dir.filePath(QStringLiteral(".qscreenrec_write_test")));
    if (!probe.open(QIODevice::WriteOnly))
        return false;
    probe.close();
    probe.remove();
    return true;
}

constexpr qint64 kMinFreeBytesToStart = 300LL * 1024 * 1024;  // 開始錄影至少需 300 MB
constexpr qint64 kMinFreeBytesWhileRec = 150LL * 1024 * 1024; // 低於 150 MB 自動停止並存檔

// 執行 ffmpeg 並等待結束（GIF 轉檔用）
bool runFfmpeg(const QString &exe, const QStringList &args, QString *error)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(exe, args);
    if (!p.waitForStarted(10000)) {
        if (error) *error = p.errorString();
        return false;
    }
    if (!p.waitForFinished(30 * 60 * 1000)) {
        p.kill();
        p.waitForFinished(3000);
        if (error) *error = QStringLiteral("timeout");
        return false;
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        if (error) *error = QString::fromLocal8Bit(p.readAll().right(600)).trimmed();
        return false;
    }
    return true;
}
} // namespace

bool Recorder::start(const AppSettings &settings, const QString &ffmpegPath, const QString &encoderId, QString *error)
{
    if (m_state != State::Idle)
        return false;
    if (m_thread.joinable())
        m_thread.join();
    if (ffmpegPath.isEmpty() || !QFileInfo(ffmpegPath).isFile()) {
        if (error) *error = tr("FFmpeg was not found. Please set its location in Settings.");
        return false;
    }

    // 儲存資料夾：設定的資料夾不能用時（例如另一台電腦沒有 D 槽），改用「影片\qScreenRec」
    QString outDir = settings.outputDir;
    m_usedFallback = false;
    if (!ensureWritableDir(outDir)) {
        outDir = AppSettings::defaultOutputDir();
        m_usedFallback = true;
        if (!ensureWritableDir(outDir)) {
            if (error) *error = tr("Cannot create the output folder:\n%1").arg(QDir::toNativeSeparators(settings.outputDir));
            return false;
        }
    }

    QStorageInfo storage(outDir);
    if (storage.isValid() && storage.bytesAvailable() >= 0 && storage.bytesAvailable() < kMinFreeBytesToStart) {
        if (error) *error = tr("Not enough free disk space in:\n%1\n\nAt least 300 MB is required.").arg(QDir::toNativeSeparators(outDir));
        return false;
    }

    const QDir dir(outDir);
    const QString base = QStringLiteral("qScreenRec_%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    m_file = uniqueFile(dir, base, settings.fileExtension());
    m_stop = false;
    m_elapsedMs = 0;
    m_backend = 0;
    setState(State::Recording);
    AppSettings job = settings;
    job.outputDir = outDir;
    m_thread = std::thread(&Recorder::run, this, Job{ job, QDir::toNativeSeparators(ffmpegPath), encoderId, m_file });
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
    StopReason reason = StopReason::User;
    QString error;

    auto finish = [&](bool ok) {
        QMetaObject::invokeMethod(this, [this, file = job.file, ok, reason, error]() {
            if (m_thread.joinable())
                m_thread.join();
            setState(State::Idle);
            emit finished(file, ok, reason, error);
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
    const bool startedWithDxgi = cap->backendName() == QLatin1String("DXGI");
    m_backend = startedWithDxgi ? 1 : 2;

    // 取得第一張完整畫面（最多等 0.5 秒）
    QElapsedTimer firstFrame;
    firstFrame.start();
    Capturer::Status st = cap->acquire(100);
    while (st != Capturer::Status::NewFrame && firstFrame.elapsed() < 500 && !m_stop)
        st = cap->acquire(100);

    const QSize frameSize = cap->image().size();
    Canvas canvas;
    if (frameSize.isEmpty() || !canvas.create(frameSize)) {
        error = tr("Failed to allocate the frame buffer.");
        finish(false);
        return;
    }
    canvas.updateDesktop(cap->image());

    Watermark watermark;
    if (s.watermarkEnabled)
        watermark.configure(s.watermarkText, s.watermarkPos, s.watermarkOpacity, frameSize);

    // 2. 啟動 ffmpeg。GIF 先錄成高畫質 MKV 暫存檔，停止後再轉 GIF，避免長時間錄影把記憶體吃光
    const bool gif = s.format == VideoFormat::Gif;
    const int fps = gif ? qMin(s.fps, 15) : s.fps;
    AppSettings enc = s;
    QString encodeTarget = job.file;
    if (gif) {
        enc.format = VideoFormat::Mkv;
        enc.quality = Quality::High;
        encodeTarget = job.file + QStringLiteral(".part.mkv");
    }
    PipeProcess ff;
    if (!ff.start(job.ffmpegPath, FFmpeg::buildArgs(enc, job.encoderId, frameSize, fps, encodeTarget), &error)) {
        finish(false);
        return;
    }

    // 3. 主迴圈：依影格率定時取「最新」畫面。畫面沒變時重複上一張，確保影片長度與實際時間一致。
    //    在兩格之間睡眠，而不是每次桌面一更新就複製，避免高更新率螢幕（120/144 Hz）浪費 CPU/GPU。
    timeBeginPeriod(1);
    const double interval = 1000.0 / fps;
    const qint64 limitMs = s.durationLimitMin > 0 ? qint64(s.durationLimitMin) * 60 * 1000 : 0;
    qint64 written = 0;
    qint64 lastReopen = -1000000;
    qint64 lastDxgiRetry = 0;
    qint64 lastDiskCheck = 0;
    int reopenFailures = 0;
    bool writeFailed = false;
    QStorageInfo storage(QFileInfo(encodeTarget).absolutePath());
    QElapsedTimer timer;
    timer.start();

    while (!m_stop) {
        qint64 now = timer.elapsed();
        if (limitMs > 0 && now >= limitMs) {
            reason = StopReason::TimeLimit;
            break;
        }
        const double nextDue = written * interval;
        if (now < nextDue) {
            Sleep(static_cast<DWORD>(qBound<double>(1.0, nextDue - now, 50.0)));
            continue;
        }

        // 每 5 秒檢查一次磁碟空間
        if (now - lastDiskCheck >= 5000) {
            lastDiskCheck = now;
            storage.refresh();
            if (storage.isValid() && storage.bytesAvailable() >= 0 && storage.bytesAvailable() < kMinFreeBytesWhileRec) {
                reason = StopReason::DiskFull;
                break;
            }
        }

        st = cap->acquire(0);
        if (st == Capturer::Status::NewFrame) {
            canvas.updateDesktop(cap->image());
            reopenFailures = 0;
        } else if (st == Capturer::Status::Lost || st == Capturer::Status::Failed) {
            // 解析度變更、鎖定畫面、UAC 等：每 0.5 秒重試一次；期間持續輸出上一張畫面
            if (now - lastReopen >= 500) {
                lastReopen = now;
                QString e;
                if (cap->open(monitor, &e)) {
                    reopenFailures = 0;
                    if (cap->acquire(50) == Capturer::Status::NewFrame)
                        canvas.updateDesktop(cap->image());
                } else if (++reopenFailures >= 10 && m_backend == 1) {
                    auto gdi = std::make_unique<GdiCapturer>();
                    if (gdi->open(monitor, &e)) {
                        cap = std::move(gdi);
                        m_backend = 2;
                        reopenFailures = 0;
                        lastDxgiRetry = now;
                    }
                }
            }
        } else if (m_backend == 2 && startedWithDxgi && now - lastDxgiRetry >= 5000) {
            // 暫時改用 GDI 後，每 5 秒嘗試切回效能較好的 DXGI
            lastDxgiRetry = now;
            auto dxgi = std::make_unique<DxgiCapturer>();
            QString e;
            if (dxgi->open(monitor, &e)) {
                cap = std::move(dxgi);
                m_backend = 1;
            }
        }

        canvas.beginFrame();
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

        now = timer.elapsed();
        const qint64 due = static_cast<qint64>(now / interval) + 1;
        qint64 count = qMax<qint64>(1, due - written);
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
    timeEndPeriod(1);

    // 4. 收尾
    cap.reset();
    const int code = ff.finish(10 * 60 * 1000);
    QFileInfo fi(encodeTarget);
    bool ok = code == 0 && !writeFailed && fi.exists() && fi.size() > 0;
    if (!ok) {
        error = tr("FFmpeg reported an error (exit code %1).").arg(code);
        const QString tail = ff.logTail();
        if (!tail.isEmpty())
            error += QStringLiteral("\n\n") + tail;
        if (reason == StopReason::DiskFull)
            error = tr("The disk is full.") + QStringLiteral("\n\n") + error;
    }

    if (ok && gif) {
        const QString palette = job.file + QStringLiteral(".palette.png");
        QString e;
        ok = runFfmpeg(job.ffmpegPath, FFmpeg::gifPaletteArgs(encodeTarget, fps, palette), &e)
             && runFfmpeg(job.ffmpegPath, FFmpeg::gifEncodeArgs(encodeTarget, palette, fps, job.file), &e);
        QFile::remove(palette);
        if (ok) {
            QFile::remove(encodeTarget);
        } else {
            // 轉 GIF 失敗時保留 MKV，至少影片不會遺失
            const QString keep = job.file.left(job.file.size() - 4) + QStringLiteral(".mkv");
            QFile::rename(encodeTarget, keep);
            error = tr("Converting to GIF failed. The recording was kept as:\n%1").arg(keep);
            if (!e.isEmpty())
                error += QStringLiteral("\n\n") + e;
        }
    }
    finish(ok);
}
