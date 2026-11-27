#include "core/settings.h"

#include <QDir>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>

namespace {
template <typename E>
E readEnum(const QSettings &s, const char *key, E def, int maxValue)
{
    bool ok = false;
    const int v = s.value(key, static_cast<int>(def)).toInt(&ok);
    if (!ok || v < 0 || v > maxValue)
        return def;
    return static_cast<E>(v);
}
} // namespace

QString AppSettings::defaultOutputDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (base.isEmpty())
        base = QDir::homePath();
    return QDir(base).filePath(QStringLiteral("qScreenRec"));
}

QString AppSettings::systemLanguage()
{
    const QLocale loc = QLocale::system();
    return loc.language() == QLocale::Chinese ? QStringLiteral("zh_TW") : QStringLiteral("en");
}

QString AppSettings::effectiveLanguage() const
{
    return language.isEmpty() ? systemLanguage() : language;
}

AppSettings AppSettings::load()
{
    QSettings s; // 非 const：load 時會寫入一次性的遷移旗標
    AppSettings a;
    a.monitorIndex = s.value("record/monitor", 0).toInt();
    a.format = readEnum(s, "record/format", VideoFormat::Mp4, 2);
    a.resolution = readEnum(s, "record/resolution", Resolution::Original, 5);
    a.fps = qBound(5, s.value("record/fps", 30).toInt(), 60);
    a.quality = readEnum(s, "record/quality", Quality::High, 2);
    a.drawCursor = s.value("record/drawCursor", true).toBool();
    a.highlightClicks = s.value("record/highlightClicks", false).toBool();
    a.durationLimitMin = qBound(0, s.value("record/durationLimitMin", 0).toInt(), 24 * 60);
    a.countdownSec = qBound(0, s.value("record/countdownSec", 3).toInt(), 10);
    a.outputDir = s.value("record/outputDir", defaultOutputDir()).toString();
    a.encoder = s.value("record/encoder").toString();

    a.watermarkEnabled = s.value("watermark/enabled", false).toBool();
    a.watermarkText = s.value("watermark/text").toString().left(kWatermarkMaxChars);
    a.watermarkPos = readEnum(s, "watermark/position", WatermarkPosition::BottomRight, 4);
    a.watermarkOpacity = qBound(10, s.value("watermark/opacity", 70).toInt(), 100);

    const QString hk = s.value("general/hotkey").toString();
    if (!hk.isEmpty())
        a.hotkey = QKeySequence::fromString(hk, QKeySequence::PortableText);
    // 1.0.0 的預設 Ctrl+Shift+R 與瀏覽器「強制重新整理」相同，自動改為新預設（只做一次）
    if (!s.value("general/hotkeyMigrated", false).toBool()) {
        if (a.hotkey == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R))
            a.hotkey = AppSettings().hotkey;
        s.setValue("general/hotkeyMigrated", true);
    }
    a.language = s.value("general/language").toString();
    a.autostart = s.value("general/autostart", false).toBool();
    a.notifyOnSave = s.value("general/notifyOnSave", true).toBool();
    a.ffmpegPath = s.value("general/ffmpegPath").toString();
    if (a.outputDir.isEmpty())
        a.outputDir = defaultOutputDir();
    return a;
}

void AppSettings::save() const
{
    QSettings s;
    s.setValue("record/monitor", monitorIndex);
    s.setValue("record/format", static_cast<int>(format));
    s.setValue("record/resolution", static_cast<int>(resolution));
    s.setValue("record/fps", fps);
    s.setValue("record/quality", static_cast<int>(quality));
    s.setValue("record/drawCursor", drawCursor);
    s.setValue("record/highlightClicks", highlightClicks);
    s.setValue("record/durationLimitMin", durationLimitMin);
    s.setValue("record/countdownSec", countdownSec);
    s.setValue("record/outputDir", outputDir);
    s.setValue("record/encoder", encoder);

    s.setValue("watermark/enabled", watermarkEnabled);
    s.setValue("watermark/text", watermarkText.left(kWatermarkMaxChars));
    s.setValue("watermark/position", static_cast<int>(watermarkPos));
    s.setValue("watermark/opacity", watermarkOpacity);

    s.setValue("general/hotkey", hotkey.toString(QKeySequence::PortableText));
    s.setValue("general/language", language);
    s.setValue("general/autostart", autostart);
    s.setValue("general/notifyOnSave", notifyOnSave);
    s.setValue("general/ffmpegPath", ffmpegPath);
}

QString AppSettings::fileExtension() const
{
    switch (format) {
    case VideoFormat::Mkv: return QStringLiteral("mkv");
    case VideoFormat::Gif: return QStringLiteral("gif");
    case VideoFormat::Mp4:
    default: return QStringLiteral("mp4");
    }
}

int AppSettings::targetHeight() const
{
    switch (resolution) {
    case Resolution::P2160: return 2160;
    case Resolution::P1440: return 1440;
    case Resolution::P1080: return 1080;
    case Resolution::P720: return 720;
    case Resolution::P480: return 480;
    case Resolution::Original:
    default: return 0;
    }
}
