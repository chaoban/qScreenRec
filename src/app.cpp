#include "app.h"

#include "core/autostart.h"
#include "core/hotkey.h"
#include "capture/capturer.h"
#include "ui/aboutdialog.h"
#include "ui/helpdialog.h"
#include "ui/settingsdialog.h"
#include "ui/theme.h"
#include "version.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLibraryInfo>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QScreen>
#include <QUrl>
#include <thread>

App::App(bool startMinimized, QObject *parent) : QObject(parent)
{
    m_settings = AppSettings::load();
    applyLanguage();

    m_recorder = new Recorder(this);
    connect(m_recorder, &Recorder::stateChanged, this, &App::onRecorderState);
    connect(m_recorder, &Recorder::finished, this, &App::onRecordingFinished);

    m_hotkey = new GlobalHotkey(this);
    connect(m_hotkey, &GlobalHotkey::activated, this, &App::toggleRecording);

    // 系統匣選單
    m_menu = new QMenu;
    m_actStatus = m_menu->addAction(QString());
    m_actStatus->setEnabled(false);
    m_menu->addSeparator();
    m_actRecord = m_menu->addAction(QString(), this, &App::toggleRecording);
    m_actFolder = m_menu->addAction(QString(), this, &App::openOutputFolder);
    m_menu->addSeparator();
    m_actSettings = m_menu->addAction(QString(), this, &App::showSettings);
    m_actHelp = m_menu->addAction(QString(), this, &App::showHelp);
    m_actAbout = m_menu->addAction(QString(), this, &App::showAbout);
    m_menu->addSeparator();
    m_actQuit = m_menu->addAction(QString(), this, &App::quit);

    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/app.png")), this);
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::DoubleClick)
            showSettings();
        else if (r == QSystemTrayIcon::Trigger)
            m_menu->popup(QCursor::pos());
    });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
        if (!m_lastFile.isEmpty() && QFileInfo::exists(m_lastFile))
            QProcess::startDetached(QStringLiteral("explorer.exe"),
                                    { QStringLiteral("/select,"), QDir::toNativeSeparators(m_lastFile) });
    });
    m_tray->show();

    m_tick.setInterval(500);
    connect(&m_tick, &QTimer::timeout, this, &App::updateTray);
    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, [this] {
        if (--m_countdownLeft <= 0) {
            m_countdownTimer.stop();
            if (m_countdownLabel)
                m_countdownLabel->deleteLater();
            // 等倒數視窗從畫面上消失後再開始，避免被錄進去
            QTimer::singleShot(200, this, &App::startRecording);
        } else if (m_countdownLabel) {
            m_countdownLabel->setText(QString::number(m_countdownLeft));
        }
    });

    qApp->installEventFilter(this); // F1 = 使用說明

    retranslate();
    registerHotkey(true);
    if (m_settings.autostart != AutoStart::isEnabled())
        AutoStart::setEnabled(m_settings.autostart);
    refreshFfmpeg(m_settings.ffmpegPath);

    if (!startMinimized) {
        m_tray->showMessage(QStringLiteral(APP_NAME),
                            tr("%1 is running in the system tray. Press %2 to start or stop recording.")
                                .arg(QStringLiteral(APP_NAME), hotkeyText()),
                            QSystemTrayIcon::Information, 5000);
    }
}

App::~App()
{
    delete m_menu;
}

QString App::hotkeyText() const
{
    return m_settings.hotkey.toString(QKeySequence::NativeText);
}

void App::applyLanguage()
{
    qApp->removeTranslator(&m_appTranslator);
    qApp->removeTranslator(&m_qtTranslator);
    const QString lang = m_settings.effectiveLanguage();
    if (lang == QLatin1String("zh_TW")) {
        if (m_appTranslator.load(QStringLiteral(":/i18n/qscreenrec_zh_TW.qm")))
            qApp->installTranslator(&m_appTranslator);
        const QStringList dirs = {
            QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath),
        };
        for (const QString &d : dirs) {
            if (m_qtTranslator.load(QStringLiteral("qtbase_zh_TW"), d)) {
                qApp->installTranslator(&m_qtTranslator);
                break;
            }
        }
    }
}

void App::retranslate()
{
    m_actFolder->setText(tr("Open recordings folder"));
    m_actSettings->setText(tr("Settings…"));
    m_actHelp->setText(tr("Help") + QStringLiteral("\tF1"));
    m_actAbout->setText(tr("About %1").arg(QStringLiteral(APP_NAME)));
    m_actQuit->setText(tr("Exit"));
    updateTray();
}

void App::registerHotkey(bool warnOnFailure)
{
    if (!m_hotkey->setShortcut(m_settings.hotkey) && warnOnFailure) {
        m_tray->showMessage(QStringLiteral(APP_NAME),
                            tr("The hotkey %1 is already used by another program. Please choose another one in Settings.")
                                .arg(hotkeyText()),
                            QSystemTrayIcon::Warning, 8000);
    }
}

void App::refreshFfmpeg(const QString &preferredPath)
{
    m_ffmpegPath = FFmpeg::locate(preferredPath);
    m_ffmpegVersion.clear();
    m_encoders.clear();
    m_probing = !m_ffmpegPath.isEmpty();
    const int gen = ++m_probeGeneration;
    if (m_settingsDlg) {
        m_settingsDlg->setFfmpegStatus(m_ffmpegPath, QString());
        m_settingsDlg->setEncoders({}, m_probing);
    }
    if (!m_probing)
        return;

    const QString path = m_ffmpegPath;
    QPointer<App> self(this);
    std::thread([self, path, gen] {
        const QString version = FFmpeg::version(path);
        const QList<EncoderInfo> encoders = FFmpeg::probeEncoders(path);
        if (App *app = self.data()) {
            QMetaObject::invokeMethod(app, [app, gen, version, encoders] {
                app->onProbeDone(gen, version, encoders);
            }, Qt::QueuedConnection);
        }
    }).detach();
}

void App::onProbeDone(int generation, const QString &version, const QList<EncoderInfo> &encoders)
{
    if (generation != m_probeGeneration)
        return;
    m_probing = false;
    m_ffmpegVersion = version;
    m_encoders = encoders;
    if (m_settingsDlg) {
        m_settingsDlg->setFfmpegStatus(m_ffmpegPath, m_ffmpegVersion);
        m_settingsDlg->setEncoders(m_encoders, false);
    }
}

QString App::pickEncoder() const
{
    if (!m_settings.encoder.isEmpty()) {
        for (const EncoderInfo &e : m_encoders) {
            if (e.id == m_settings.encoder)
                return e.id;
        }
    }
    return m_encoders.isEmpty() ? QStringLiteral("mpeg4") : m_encoders.first().id;
}

void App::toggleRecording()
{
    if (m_countdownTimer.isActive()) { // 倒數中再按一次 = 取消
        m_countdownTimer.stop();
        if (m_countdownLabel)
            m_countdownLabel->deleteLater();
        updateTray();
        return;
    }
    switch (m_recorder->state()) {
    case Recorder::State::Recording:
        m_recorder->stop();
        break;
    case Recorder::State::Finishing:
        break;
    case Recorder::State::Idle:
        if (m_ffmpegPath.isEmpty()) {
            QMessageBox::warning(nullptr, QStringLiteral(APP_NAME),
                                 tr("FFmpeg was not found.\n\nPlace ffmpeg.exe next to qScreenRec.exe, "
                                    "or set its location in Settings → General."));
            showSettings();
            return;
        }
        if (m_probing) {
            m_tray->showMessage(QStringLiteral(APP_NAME), tr("Detecting video encoders, please try again in a moment."),
                                QSystemTrayIcon::Information, 3000);
            return;
        }
        if (m_settings.countdownSec > 0)
            startCountdown();
        else
            startRecording();
        break;
    }
}

void App::startCountdown()
{
    // 在要錄影的螢幕中央顯示倒數數字
    QScreen *screen = QGuiApplication::primaryScreen();
    const QList<MonitorInfo> monitors = Capturer::monitors();
    if (m_settings.monitorIndex >= 0 && m_settings.monitorIndex < monitors.size()) {
        const QString dev = monitors.at(m_settings.monitorIndex).deviceName;
        for (QScreen *s : QGuiApplication::screens()) {
            if (s->name() == dev)
                screen = s;
        }
    }
    auto *label = new QLabel;
    label->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowTransparentForInput);
    label->setAttribute(Qt::WA_TranslucentBackground);
    label->setAttribute(Qt::WA_ShowWithoutActivating);
    label->setAttribute(Qt::WA_DeleteOnClose);
    label->setAlignment(Qt::AlignCenter);
    label->setFixedSize(220, 220);
    label->setStyleSheet(QStringLiteral("QLabel { background: rgba(20,20,22,200); color: %1; border-radius: 110px;"
                                        " font-size: 96pt; font-weight: bold; }").arg(QLatin1String(Theme::kAccent)));
    m_countdownLeft = m_settings.countdownSec;
    label->setText(QString::number(m_countdownLeft));
    const QRect g = screen->geometry();
    label->move(g.center() - QPoint(110, 110));
    label->show();
    m_countdownLabel = label;
    m_countdownTimer.start();
    updateTray();
}

void App::startRecording()
{
    QString error;
    if (!m_recorder->start(m_settings, m_ffmpegPath, pickEncoder(), &error)) {
        QMessageBox::critical(nullptr, QStringLiteral(APP_NAME), tr("Could not start recording.\n\n%1").arg(error));
    }
}

void App::onRecorderState(Recorder::State state)
{
    if (state == Recorder::State::Recording)
        m_tick.start();
    else if (state == Recorder::State::Idle)
        m_tick.stop();
    updateTray();
}

void App::onRecordingFinished(const QString &file, bool ok, bool limitReached, const QString &error)
{
    updateTray();
    if (ok) {
        m_lastFile = file;
        if (m_settings.notifyOnSave || limitReached) {
            QString msg = tr("Saved: %1").arg(QFileInfo(file).fileName());
            if (limitReached)
                msg = tr("Recording time limit reached.") + QLatin1Char('\n') + msg;
            msg += QLatin1Char('\n') + tr("Click to show it in the folder.");
            m_tray->showMessage(QStringLiteral(APP_NAME), msg, QSystemTrayIcon::Information, 6000);
        }
    } else {
        QMessageBox::critical(nullptr, QStringLiteral(APP_NAME), tr("Recording failed.\n\n%1").arg(error));
    }
    if (m_quitWhenDone)
        qApp->quit();
}

void App::updateTray()
{
    const Recorder::State st = m_recorder->state();
    const bool recording = st == Recorder::State::Recording;
    QString status;
    if (recording) {
        const qint64 sec = m_recorder->elapsedMs() / 1000;
        const QString time = QStringLiteral("%1:%2:%3")
                                 .arg(sec / 3600, 2, 10, QLatin1Char('0'))
                                 .arg((sec / 60) % 60, 2, 10, QLatin1Char('0'))
                                 .arg(sec % 60, 2, 10, QLatin1Char('0'));
        status = tr("Recording %1").arg(time);
        if (m_settings.durationLimitMin > 0)
            status += tr(" / limit %1 min").arg(m_settings.durationLimitMin);
        m_blink = !m_blink;
        m_tray->setIcon(QIcon(m_blink ? QStringLiteral(":/icons/recording.png") : QStringLiteral(":/icons/app.png")));
        m_actRecord->setText(tr("Stop recording") + QLatin1Char('\t') + hotkeyText());
    } else if (st == Recorder::State::Finishing) {
        status = tr("Saving…");
        m_tray->setIcon(QIcon(QStringLiteral(":/icons/recording.png")));
        m_actRecord->setText(tr("Saving…"));
    } else if (m_countdownTimer.isActive()) {
        status = tr("Starting in %1…").arg(m_countdownLeft);
        m_actRecord->setText(tr("Cancel countdown") + QLatin1Char('\t') + hotkeyText());
    } else {
        status = tr("Ready");
        m_tray->setIcon(QIcon(QStringLiteral(":/icons/app.png")));
        m_actRecord->setText(tr("Start recording") + QLatin1Char('\t') + hotkeyText());
    }
    m_actRecord->setEnabled(st != Recorder::State::Finishing);
    m_actStatus->setText(QStringLiteral("%1 – %2").arg(QStringLiteral(APP_NAME), status));
    m_tray->setToolTip(QStringLiteral("%1\n%2").arg(QStringLiteral(APP_NAME), status));
}

void App::showSettings()
{
    if (m_settingsDlg) {
        m_settingsDlg->raise();
        m_settingsDlg->activateWindow();
        return;
    }
    auto *dlg = new SettingsDialog(m_settings);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    m_settingsDlg = dlg;
    dlg->setFfmpegStatus(m_ffmpegPath, m_ffmpegVersion);
    dlg->setEncoders(m_encoders, m_probing);
    connect(dlg, &SettingsDialog::helpRequested, this, &App::showHelp);
    connect(dlg, &SettingsDialog::ffmpegPathChanged, this, &App::refreshFfmpeg);
    connect(dlg, &QDialog::accepted, this, [this, dlg] {
        const AppSettings old = m_settings;
        m_settings = dlg->settings();
        m_settings.save();
        if (old.autostart != m_settings.autostart)
            AutoStart::setEnabled(m_settings.autostart);
        if (old.hotkey != m_settings.hotkey)
            registerHotkey(true);
        if (old.ffmpegPath != m_settings.ffmpegPath)
            refreshFfmpeg(m_settings.ffmpegPath);
        if (old.effectiveLanguage() != m_settings.effectiveLanguage()) {
            applyLanguage();
            retranslate();
            if (m_helpDlg)
                m_helpDlg->load(m_settings.effectiveLanguage(), hotkeyText());
        }
        updateTray();
    });
    connect(dlg, &QDialog::rejected, this, [this, dlg] {
        // 取消時若改過 FFmpeg 路徑，恢復原設定
        if (dlg->settings().ffmpegPath != m_settings.ffmpegPath)
            refreshFfmpeg(m_settings.ffmpegPath);
    });
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void App::showHelp()
{
    if (!m_helpDlg) {
        m_helpDlg = new HelpDialog;
        m_helpDlg->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    m_helpDlg->load(m_settings.effectiveLanguage(), hotkeyText());
    m_helpDlg->show();
    m_helpDlg->raise();
    m_helpDlg->activateWindow();
}

void App::showAbout()
{
    if (m_aboutDlg) {
        m_aboutDlg->raise();
        m_aboutDlg->activateWindow();
        return;
    }
    auto *dlg = new AboutDialog(m_settings.effectiveLanguage(), m_ffmpegVersion);
    m_aboutDlg = dlg;
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void App::openOutputFolder()
{
    QDir().mkpath(m_settings.outputDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_settings.outputDir));
}

void App::quit()
{
    if (m_recorder->state() != Recorder::State::Idle) {
        m_quitWhenDone = true;
        m_recorder->stop();
        return;
    }
    qApp->quit();
}

bool App::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_F1 && ke->modifiers() == Qt::NoModifier) {
            showHelp();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}
