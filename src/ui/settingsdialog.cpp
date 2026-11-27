#include "ui/settingsdialog.h"

#include "capture/capturer.h"
#include "core/autostart.h"
#include "core/watermark.h"
#include "ui/hotkeyedit.h"
#include "ui/theme.h"
#include "version.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
void selectData(QComboBox *box, const QVariant &v)
{
    const int i = box->findData(v);
    box->setCurrentIndex(i >= 0 ? i : 0);
}
} // namespace

SettingsDialog::SettingsDialog(const AppSettings &settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(tr("%1 Settings").arg(QStringLiteral(APP_NAME)));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setMinimumWidth(520);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildRecordingTab(), tr("Recording"));
    tabs->addTab(buildWatermarkTab(), tr("Watermark"));
    tabs->addTab(buildGeneralTab(), tr("General"));

    auto *buttons = new QDialogButtonBox(this);
    auto *help = buttons->addButton(tr("Help (F1)"), QDialogButtonBox::HelpRole);
    auto *ok = buttons->addButton(tr("OK"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    ok->setProperty("accent", true);
    ok->setDefault(true);
    connect(help, &QPushButton::clicked, this, &SettingsDialog::helpRequested);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    updateFormatDependent();
    updateWatermarkPreview();
    Theme::darkTitleBar(this);
}

QWidget *SettingsDialog::buildRecordingTab()
{
    auto *w = new QWidget;
    auto *form = new QFormLayout(w);
    form->setLabelAlignment(Qt::AlignRight);

    m_monitor = new QComboBox;
    for (const MonitorInfo &m : Capturer::monitors())
        m_monitor->addItem(m.displayName(), m.index);
    selectData(m_monitor, m_settings.monitorIndex);
    form->addRow(tr("Monitor:"), m_monitor);

    m_format = new QComboBox;
    m_format->addItem(tr("MP4 (H.264) – recommended"), int(VideoFormat::Mp4));
    m_format->addItem(tr("MKV (H.264) – safe if interrupted"), int(VideoFormat::Mkv));
    m_format->addItem(tr("GIF animation (max 15 fps)"), int(VideoFormat::Gif));
    selectData(m_format, int(m_settings.format));
    connect(m_format, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateFormatDependent);
    form->addRow(tr("Format:"), m_format);

    m_resolution = new QComboBox;
    m_resolution->addItem(tr("Original"), int(Resolution::Original));
    m_resolution->addItem(QStringLiteral("2160p (4K)"), int(Resolution::P2160));
    m_resolution->addItem(QStringLiteral("1440p (2K)"), int(Resolution::P1440));
    m_resolution->addItem(QStringLiteral("1080p (Full HD)"), int(Resolution::P1080));
    m_resolution->addItem(QStringLiteral("720p (HD)"), int(Resolution::P720));
    m_resolution->addItem(QStringLiteral("480p"), int(Resolution::P480));
    selectData(m_resolution, int(m_settings.resolution));
    m_resolution->setToolTip(tr("Scaled to this height, keeping the aspect ratio. Never upscaled."));
    form->addRow(tr("Resolution:"), m_resolution);

    m_fps = new QComboBox;
    for (int f : { 10, 15, 24, 30, 60 })
        m_fps->addItem(tr("%1 fps").arg(f), f);
    selectData(m_fps, m_settings.fps);
    form->addRow(tr("Frame rate:"), m_fps);

    m_quality = new QComboBox;
    m_quality->addItem(tr("High"), int(Quality::High));
    m_quality->addItem(tr("Medium"), int(Quality::Medium));
    m_quality->addItem(tr("Low (smaller file)"), int(Quality::Low));
    selectData(m_quality, int(m_settings.quality));
    form->addRow(tr("Quality:"), m_quality);

    m_encoder = new QComboBox;
    form->addRow(tr("Encoder:"), m_encoder);

    m_duration = new QSpinBox;
    m_duration->setRange(0, 24 * 60);
    m_duration->setSpecialValueText(tr("Unlimited"));
    m_duration->setSuffix(tr(" min"));
    m_duration->setValue(m_settings.durationLimitMin);
    m_duration->setToolTip(tr("Recording stops automatically after this time. 0 = unlimited."));
    form->addRow(tr("Recording time:"), m_duration);

    m_countdown = new QSpinBox;
    m_countdown->setRange(0, 10);
    m_countdown->setSpecialValueText(tr("None"));
    m_countdown->setSuffix(tr(" s"));
    m_countdown->setValue(m_settings.countdownSec);
    form->addRow(tr("Countdown:"), m_countdown);

    m_cursor = new QCheckBox(tr("Record mouse cursor"));
    m_cursor->setChecked(m_settings.drawCursor);
    m_clicks = new QCheckBox(tr("Highlight mouse clicks"));
    m_clicks->setChecked(m_settings.highlightClicks);
    connect(m_cursor, &QCheckBox::toggled, m_clicks, &QCheckBox::setEnabled);
    m_clicks->setEnabled(m_settings.drawCursor);
    auto *mouseBox = new QVBoxLayout;
    mouseBox->addWidget(m_cursor);
    mouseBox->addWidget(m_clicks);
    form->addRow(tr("Mouse:"), mouseBox);

    m_outputDir = new QLineEdit(QDir::toNativeSeparators(m_settings.outputDir));
    auto *browse = new QPushButton(tr("Browse…"));
    auto *open = new QPushButton(tr("Open"));
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, tr("Choose output folder"), m_outputDir->text());
        if (!d.isEmpty())
            m_outputDir->setText(QDir::toNativeSeparators(d));
    });
    connect(open, &QPushButton::clicked, this, [this] {
        QDir().mkpath(m_outputDir->text());
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputDir->text()));
    });
    auto *dirRow = new QHBoxLayout;
    dirRow->addWidget(m_outputDir, 1);
    dirRow->addWidget(browse);
    dirRow->addWidget(open);
    form->addRow(tr("Save to:"), dirRow);
    return w;
}

QWidget *SettingsDialog::buildWatermarkTab()
{
    auto *w = new QWidget;
    auto *form = new QFormLayout(w);
    form->setLabelAlignment(Qt::AlignRight);

    m_wmEnabled = new QCheckBox(tr("Add a text watermark to recordings"));
    m_wmEnabled->setChecked(m_settings.watermarkEnabled);
    form->addRow(QString(), m_wmEnabled);

    m_wmText = new QLineEdit(m_settings.watermarkText);
    m_wmText->setMaxLength(AppSettings::kWatermarkMaxChars);
    m_wmText->setPlaceholderText(tr("Up to %1 characters").arg(AppSettings::kWatermarkMaxChars));
    m_wmCounter = new QLabel;
    m_wmCounter->setMinimumWidth(48);
    auto *textRow = new QHBoxLayout;
    textRow->addWidget(m_wmText, 1);
    textRow->addWidget(m_wmCounter);
    form->addRow(tr("Text:"), textRow);

    m_wmPos = new QComboBox;
    m_wmPos->addItem(tr("Bottom right"), int(WatermarkPosition::BottomRight));
    m_wmPos->addItem(tr("Bottom left"), int(WatermarkPosition::BottomLeft));
    m_wmPos->addItem(tr("Top right"), int(WatermarkPosition::TopRight));
    m_wmPos->addItem(tr("Top left"), int(WatermarkPosition::TopLeft));
    m_wmPos->addItem(tr("Center"), int(WatermarkPosition::Center));
    selectData(m_wmPos, int(m_settings.watermarkPos));
    form->addRow(tr("Position:"), m_wmPos);

    m_wmOpacity = new QSlider(Qt::Horizontal);
    m_wmOpacity->setRange(10, 100);
    m_wmOpacity->setValue(m_settings.watermarkOpacity);
    m_wmOpacityLabel = new QLabel;
    m_wmOpacityLabel->setMinimumWidth(48);
    auto *opRow = new QHBoxLayout;
    opRow->addWidget(m_wmOpacity, 1);
    opRow->addWidget(m_wmOpacityLabel);
    form->addRow(tr("Opacity:"), opRow);

    m_wmPreview = new QLabel;
    m_wmPreview->setFixedSize(400, 225);
    m_wmPreview->setStyleSheet(QStringLiteral("border: 1px solid #3A3B40; border-radius: 6px;"));
    form->addRow(tr("Preview:"), m_wmPreview);

    auto refresh = [this] { updateWatermarkPreview(); };
    connect(m_wmEnabled, &QCheckBox::toggled, this, refresh);
    connect(m_wmText, &QLineEdit::textChanged, this, refresh);
    connect(m_wmPos, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_wmOpacity, &QSlider::valueChanged, this, refresh);
    return w;
}

QWidget *SettingsDialog::buildGeneralTab()
{
    auto *w = new QWidget;
    auto *form = new QFormLayout(w);
    form->setLabelAlignment(Qt::AlignRight);

    m_hotkey = new HotkeyEdit;
    m_hotkey->setKeySequence(m_settings.hotkey);
    auto *hkHint = new QLabel(tr("Click the box and press a new combination. Works even when the app is in the background."));
    hkHint->setWordWrap(true);
    hkHint->setStyleSheet(QStringLiteral("color: #9A9BA0;"));
    auto *hkBox = new QVBoxLayout;
    hkBox->addWidget(m_hotkey);
    hkBox->addWidget(hkHint);
    form->addRow(tr("Start / stop hotkey:"), hkBox);

    m_language = new QComboBox;
    m_language->addItem(tr("Follow system"), QString());
    m_language->addItem(QStringLiteral("繁體中文"), QStringLiteral("zh_TW"));
    m_language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    selectData(m_language, m_settings.language);
    form->addRow(tr("Language:"), m_language);

    m_autostart = new QCheckBox(tr("Start %1 when Windows starts").arg(QStringLiteral(APP_NAME)));
    m_autostart->setChecked(m_settings.autostart);
    if (AutoStart::isPackaged()) {
        m_autostart->setChecked(false);
        m_autostart->setEnabled(false);
        m_autostart->setToolTip(tr("For the Microsoft Store version, turn this on in Windows Settings → Apps → Startup."));
    }
    form->addRow(QString(), m_autostart);

    m_notify = new QCheckBox(tr("Show a notification when a recording is saved"));
    m_notify->setChecked(m_settings.notifyOnSave);
    form->addRow(QString(), m_notify);

    m_ffmpegPath = new QLineEdit(m_settings.ffmpegPath);
    m_ffmpegPath->setPlaceholderText(tr("Automatic (next to the program or in PATH)"));
    auto *browse = new QPushButton(tr("Browse…"));
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, tr("Locate ffmpeg.exe"), m_ffmpegPath->text(),
                                                       QStringLiteral("ffmpeg.exe (ffmpeg.exe)"));
        if (!f.isEmpty()) {
            m_ffmpegPath->setText(QDir::toNativeSeparators(f));
            emit ffmpegPathChanged(m_ffmpegPath->text());
        }
    });
    connect(m_ffmpegPath, &QLineEdit::editingFinished, this, [this] { emit ffmpegPathChanged(m_ffmpegPath->text()); });
    auto *ffRow = new QHBoxLayout;
    ffRow->addWidget(m_ffmpegPath, 1);
    ffRow->addWidget(browse);
    m_ffmpegStatus = new QLabel;
    m_ffmpegStatus->setWordWrap(true);
    m_ffmpegStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *ffBox = new QVBoxLayout;
    ffBox->addLayout(ffRow);
    ffBox->addWidget(m_ffmpegStatus);
    form->addRow(tr("FFmpeg:"), ffBox);
    return w;
}

void SettingsDialog::setEncoders(const QList<EncoderInfo> &encoders, bool probing)
{
    const QString current = m_encodersReady ? m_encoder->currentData().toString() : m_settings.encoder;
    m_encoder->clear();
    if (probing) {
        m_encoder->addItem(tr("Detecting…"), QString());
        m_encoder->setEnabled(false);
        return;
    }
    m_encodersReady = true;
    m_encoder->setEnabled(true);
    const QString best = encoders.isEmpty() ? QString() : encoders.first().displayName;
    m_encoder->addItem(best.isEmpty() ? tr("Automatic") : tr("Automatic (%1)").arg(best), QString());
    for (const EncoderInfo &e : encoders)
        m_encoder->addItem(e.displayName, e.id);
    selectData(m_encoder, current);
    updateFormatDependent();
}

void SettingsDialog::setFfmpegStatus(const QString &path, const QString &version)
{
    if (path.isEmpty()) {
        m_ffmpegStatus->setText(QStringLiteral("<span style='color:#FF6B6B'>%1</span>")
                                    .arg(tr("ffmpeg.exe not found. Recording is unavailable until it is set.")));
    } else {
        m_ffmpegStatus->setText(QStringLiteral("<span style='color:#7FD27F'>✔</span> %1<br><span style='color:#9A9BA0'>%2</span>")
                                    .arg(version.isEmpty() ? tr("FFmpeg found") : tr("FFmpeg %1").arg(version),
                                         path.toHtmlEscaped()));
    }
}

void SettingsDialog::updateFormatDependent()
{
    const bool gif = m_format->currentData().toInt() == int(VideoFormat::Gif);
    m_encoder->setEnabled(!gif && m_encoder->count() > 1);
    m_quality->setEnabled(!gif);
}

void SettingsDialog::updateWatermarkPreview()
{
    const int len = m_wmText->text().length();
    m_wmCounter->setText(QStringLiteral("%1/%2").arg(len).arg(AppSettings::kWatermarkMaxChars));
    m_wmOpacityLabel->setText(QStringLiteral("%1%").arg(m_wmOpacity->value()));
    const bool on = m_wmEnabled->isChecked();
    m_wmText->setEnabled(on);
    m_wmPos->setEnabled(on);
    m_wmOpacity->setEnabled(on);

    // 模擬一張 1080p 畫面縮小後的預覽
    const QSize frame(1920, 1080);
    QImage img(frame, QImage::Format_RGB32);
    QPainter p(&img);
    QLinearGradient g(0, 0, frame.width(), frame.height());
    g.setColorAt(0, QColor(0x3A, 0x4A, 0x6B));
    g.setColorAt(1, QColor(0x6B, 0x8C, 0xA8));
    p.fillRect(img.rect(), g);
    p.fillRect(QRect(160, 140, 1100, 700), QColor(0xF4, 0xF4, 0xF6));
    p.fillRect(QRect(160, 140, 1100, 56), QColor(0xDD, 0xDE, 0xE2));
    for (int i = 0; i < 8; ++i)
        p.fillRect(QRect(220, 260 + i * 66, 600 + (i % 3) * 120, 26), QColor(0xC8, 0xCA, 0xD0));
    if (on) {
        Watermark wm;
        wm.configure(m_wmText->text(), WatermarkPosition(m_wmPos->currentData().toInt()), m_wmOpacity->value(), frame);
        wm.paint(p);
    }
    p.end();
    m_wmPreview->setPixmap(QPixmap::fromImage(img.scaled(m_wmPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

AppSettings SettingsDialog::settings() const
{
    AppSettings s = m_settings;
    s.monitorIndex = m_monitor->currentData().toInt();
    s.format = VideoFormat(m_format->currentData().toInt());
    s.resolution = Resolution(m_resolution->currentData().toInt());
    s.fps = m_fps->currentData().toInt();
    s.quality = Quality(m_quality->currentData().toInt());
    if (m_encodersReady)
        s.encoder = m_encoder->currentData().toString();
    s.durationLimitMin = m_duration->value();
    s.countdownSec = m_countdown->value();
    s.drawCursor = m_cursor->isChecked();
    s.highlightClicks = m_clicks->isChecked();
    s.outputDir = m_outputDir->text().trimmed().isEmpty() ? AppSettings::defaultOutputDir() : m_outputDir->text().trimmed();

    s.watermarkEnabled = m_wmEnabled->isChecked() && !m_wmText->text().trimmed().isEmpty();
    s.watermarkText = m_wmText->text().left(AppSettings::kWatermarkMaxChars);
    s.watermarkPos = WatermarkPosition(m_wmPos->currentData().toInt());
    s.watermarkOpacity = m_wmOpacity->value();

    if (!m_hotkey->keySequence().isEmpty())
        s.hotkey = m_hotkey->keySequence();
    s.language = m_language->currentData().toString();
    s.autostart = m_autostart->isChecked();
    s.notifyOnSave = m_notify->isChecked();
    s.ffmpegPath = m_ffmpegPath->text().trimmed();
    return s;
}
