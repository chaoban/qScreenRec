#include "core/ffmpeg.h"
#include "core/settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

namespace {
struct Candidate
{
    const char *id;
    const char *name;
    bool hw;
};

// 依偏好順序：GPU 硬體編碼優先，其次 Windows 內建，最後 CPU 軟體編碼
// 不使用 libopenh264：自行編譯的 OpenH264 不在 Cisco 專利授權範圍內
const Candidate kCandidates[] = {
    { "h264_nvenc", "NVIDIA NVENC (GPU)", true },
    { "h264_qsv", "Intel Quick Sync (GPU)", true },
    { "h264_amf", "AMD AMF (GPU)", true },
    { "h264_mf", "Windows Media Foundation", true },
    { "libx264", "x264 (CPU)", false },
    { "mpeg4", "MPEG-4 Part 2 (CPU)", false },
};

QString pixFmtFor(const QString &enc)
{
    if (enc == QLatin1String("h264_qsv") || enc == QLatin1String("h264_amf") || enc == QLatin1String("h264_mf"))
        return QStringLiteral("nv12");
    return QStringLiteral("yuv420p");
}

QProcess::ProcessChannelMode quietMode() { return QProcess::MergedChannels; }
} // namespace

QString FFmpeg::locate(const QString &preferredPath)
{
    if (!preferredPath.isEmpty() && QFileInfo(preferredPath).isExecutable())
        return QDir::toNativeSeparators(preferredPath);

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList local = {
        appDir.filePath(QStringLiteral("ffmpeg.exe")),
        appDir.filePath(QStringLiteral("ffmpeg/ffmpeg.exe")),
        appDir.filePath(QStringLiteral("ffmpeg/bin/ffmpeg.exe")),
    };
    for (const QString &p : local) {
        if (QFileInfo::exists(p))
            return QDir::toNativeSeparators(p);
    }
    const QString inPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    return inPath.isEmpty() ? QString() : QDir::toNativeSeparators(inPath);
}

QString FFmpeg::version(const QString &ffmpegPath)
{
    if (ffmpegPath.isEmpty())
        return {};
    QProcess p;
    p.setProcessChannelMode(quietMode());
    p.start(ffmpegPath, { QStringLiteral("-hide_banner"), QStringLiteral("-version") });
    if (!p.waitForFinished(5000))
        return {};
    const QString out = QString::fromLocal8Bit(p.readAll());
    static const QRegularExpression re(QStringLiteral("ffmpeg version (\\S+)"));
    const auto m = re.match(out);
    return m.hasMatch() ? m.captured(1) : QString();
}

QList<EncoderInfo> FFmpeg::knownEncoders()
{
    QList<EncoderInfo> list;
    for (const auto &c : kCandidates)
        list.append({ QString::fromLatin1(c.id), QString::fromLatin1(c.name), c.hw });
    return list;
}

QString FFmpeg::displayName(const QString &encoderId)
{
    for (const auto &c : kCandidates) {
        if (encoderId == QLatin1String(c.id))
            return QString::fromLatin1(c.name);
    }
    return encoderId;
}

QList<EncoderInfo> FFmpeg::probeEncoders(const QString &ffmpegPath, const std::atomic_bool *cancel)
{
    auto cancelled = [cancel] { return cancel && cancel->load(); };
    // 分段等待，讓取消可以在 0.2 秒內生效
    auto waitFor = [&](QProcess &p, int timeoutMs) {
        for (int t = 0; t < timeoutMs; t += 200) {
            if (p.waitForFinished(200))
                return true;
            if (cancelled() || p.state() == QProcess::NotRunning)
                break;
        }
        if (p.state() != QProcess::NotRunning) {
            p.kill();
            p.waitForFinished(2000);
        }
        return false;
    };
    QList<EncoderInfo> result;
    if (ffmpegPath.isEmpty())
        return result;

    QProcess list;
    list.setProcessChannelMode(quietMode());
    list.start(ffmpegPath, { QStringLiteral("-hide_banner"), QStringLiteral("-encoders") });
    if (!waitFor(list, 8000))
        return result;
    const QString encoders = QString::fromLocal8Bit(list.readAll());

    for (const auto &c : kCandidates) {
        if (cancelled())
            break;
        const QString id = QString::fromLatin1(c.id);
        if (!encoders.contains(QLatin1Char(' ') + id + QLatin1Char(' ')))
            continue;
        // 實際編碼幾格黑畫面，確認硬體／驅動真的可用
        QProcess p;
        p.setProcessChannelMode(quietMode());
        p.start(ffmpegPath, {
            QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
            QStringLiteral("-f"), QStringLiteral("lavfi"),
            QStringLiteral("-i"), QStringLiteral("color=c=black:s=640x360:r=30:d=0.3"),
            QStringLiteral("-c:v"), id,
            QStringLiteral("-pix_fmt"), pixFmtFor(id),
            QStringLiteral("-b:v"), QStringLiteral("2M"),
            QStringLiteral("-f"), QStringLiteral("null"), QStringLiteral("-"),
        });
        if (waitFor(p, 10000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0)
            result.append({ id, QString::fromLatin1(c.name), c.hw });
    }
    return result;
}

QSize FFmpeg::outputSize(const QSize &source, int targetHeight)
{
    int w = source.width();
    int h = source.height();
    if (targetHeight > 0 && targetHeight < h) {
        w = static_cast<int>(std::lround(static_cast<double>(w) * targetHeight / h));
        h = targetHeight;
    }
    return QSize(w & ~1, h & ~1);
}

QStringList FFmpeg::buildArgs(const AppSettings &s, const QString &encoderId,
                              const QSize &source, int fps, const QString &outputFile)
{
    const QSize out = outputSize(source, s.targetHeight());
    const QString scale = QStringLiteral("scale=%1:%2:flags=bicubic").arg(out.width()).arg(out.height());

    QStringList a = {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-y"),
        QStringLiteral("-f"), QStringLiteral("rawvideo"),
        QStringLiteral("-pix_fmt"), QStringLiteral("bgr0"),
        QStringLiteral("-video_size"), QStringLiteral("%1x%2").arg(source.width()).arg(source.height()),
        QStringLiteral("-framerate"), QString::number(fps),
        QStringLiteral("-i"), QStringLiteral("-"),
    };

    a << QStringLiteral("-vf") << scale;

    const QString enc = encoderId.isEmpty() ? QStringLiteral("mpeg4") : encoderId;
    a << QStringLiteral("-c:v") << enc;

    if (enc == QLatin1String("mpeg4")) {
        const int q = s.quality == Quality::High ? 3 : s.quality == Quality::Medium ? 5 : 8;
        a << QStringLiteral("-q:v") << QString::number(q);
    } else {
        const double bpp = s.quality == Quality::High ? 0.12 : s.quality == Quality::Medium ? 0.07 : 0.04;
        const qint64 bitrate = qMax<qint64>(1'000'000, static_cast<qint64>(out.width() * static_cast<double>(out.height()) * fps * bpp));
        a << QStringLiteral("-b:v") << QString::number(bitrate)
          << QStringLiteral("-maxrate") << QString::number(bitrate * 3 / 2)
          << QStringLiteral("-bufsize") << QString::number(bitrate * 2);
        if (enc == QLatin1String("libx264"))
            a << QStringLiteral("-preset") << QStringLiteral("veryfast");
        else if (enc == QLatin1String("h264_nvenc"))
            a << QStringLiteral("-preset") << QStringLiteral("p4");
        else if (enc == QLatin1String("h264_qsv"))
            a << QStringLiteral("-preset") << QStringLiteral("faster");
        // 每 2 秒一個關鍵格，方便剪輯與拖曳
        a << QStringLiteral("-g") << QString::number(fps * 2);
    }
    a << QStringLiteral("-pix_fmt") << pixFmtFor(enc);

    if (s.format == VideoFormat::Mp4)
        a << QStringLiteral("-movflags") << QStringLiteral("+faststart");
    a << outputFile;
    return a;
}

QStringList FFmpeg::gifPaletteArgs(const QString &input, int fps, const QString &palette)
{
    return {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-y"),
        QStringLiteral("-i"), input,
        QStringLiteral("-vf"), QStringLiteral("fps=%1,palettegen=stats_mode=diff").arg(fps),
        palette,
    };
}

QStringList FFmpeg::gifEncodeArgs(const QString &input, const QString &palette, int fps, const QString &output)
{
    return {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-y"),
        QStringLiteral("-i"), input,
        QStringLiteral("-i"), palette,
        QStringLiteral("-lavfi"),
        QStringLiteral("fps=%1[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle").arg(fps),
        QStringLiteral("-loop"), QStringLiteral("0"),
        output,
    };
}
