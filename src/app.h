#pragma once

#include "core/ffmpeg.h"
#include "core/recorder.h"
#include "core/settings.h"

#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTranslator>
#include <atomic>
#include <thread>

class QAction;
class QMenu;
class QLabel;
class GlobalHotkey;
class SettingsDialog;
class HelpDialog;

// 常駐程式主控：系統匣、全域快捷鍵、錄影流程、各視窗
class App : public QObject
{
    Q_OBJECT
public:
    explicit App(bool startMinimized, QObject *parent = nullptr);
    ~App() override;

    void toggleRecording();
    void showSettings(int tab = -1);   // tab: 0 錄影, 1 浮水印, 2 一般
    void showHelp();
    void showAbout();
    void openOutputFolder();
    void quit();

private:
    void onProbeDone(int generation, const QString &version, const QList<EncoderInfo> &encoders);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applyLanguage();
    void retranslate();
    void registerHotkey(bool warnOnFailure);
    void refreshFfmpeg(const QString &preferredPath);
    void startCountdown();
    void startRecording();
    void onRecorderState(Recorder::State state);
    void onRecordingFinished(const QString &file, bool ok, Recorder::StopReason reason, const QString &error);
    void stopProbe();
    void updateTray();
    QString hotkeyText() const;
    QString pickEncoder() const;

    AppSettings m_settings;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_actRecord = nullptr;
    QAction *m_actFolder = nullptr;
    QAction *m_actSettings = nullptr;
    QAction *m_actHelp = nullptr;
    QAction *m_actAbout = nullptr;
    QAction *m_actQuit = nullptr;
    QAction *m_actStatus = nullptr;

    GlobalHotkey *m_hotkey = nullptr;
    Recorder *m_recorder = nullptr;
    QTranslator m_appTranslator;
    QTranslator m_qtTranslator;

    QString m_ffmpegPath;
    QString m_ffmpegVersion;
    QList<EncoderInfo> m_encoders;
    bool m_probing = false;
    std::thread m_probeThread;
    std::atomic_bool m_probeCancel { false };
    int m_probeGeneration = 0;

    QPointer<SettingsDialog> m_settingsDlg;
    QPointer<HelpDialog> m_helpDlg;
    QPointer<QWidget> m_aboutDlg;

    QIcon m_iconApp;
    QIcon m_iconRec;
    QTimer m_tick;
    bool m_blink = false;
    QPointer<QLabel> m_countdownLabel;
    QTimer m_countdownTimer;
    int m_countdownLeft = 0;
    bool m_quitWhenDone = false;
    QString m_lastFile;
};
