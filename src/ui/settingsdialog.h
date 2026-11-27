#pragma once

#include "core/ffmpeg.h"
#include "core/settings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class HotkeyEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(const AppSettings &settings, QWidget *parent = nullptr);

    AppSettings settings() const;
    void setCurrentTab(int index);
    void setEncoders(const QList<EncoderInfo> &encoders, bool probing);
    void setFfmpegStatus(const QString &path, const QString &version);

signals:
    void helpRequested();
    void ffmpegPathChanged(const QString &path);

private:
    QWidget *buildRecordingTab();
    QWidget *buildWatermarkTab();
    QWidget *buildGeneralTab();
    void updateWatermarkPreview();
    void updateFormatDependent();

    AppSettings m_settings;
    class QTabWidget *m_tabs = nullptr;
    bool m_encodersReady = false;

    // 錄影
    QComboBox *m_monitor = nullptr;
    QComboBox *m_format = nullptr;
    QComboBox *m_resolution = nullptr;
    QComboBox *m_fps = nullptr;
    QComboBox *m_quality = nullptr;
    QComboBox *m_encoder = nullptr;
    QSpinBox *m_duration = nullptr;
    QSpinBox *m_countdown = nullptr;
    QCheckBox *m_cursor = nullptr;
    QCheckBox *m_clicks = nullptr;
    QLineEdit *m_outputDir = nullptr;

    // 浮水印
    QCheckBox *m_wmEnabled = nullptr;
    QLineEdit *m_wmText = nullptr;
    QLabel *m_wmCounter = nullptr;
    QComboBox *m_wmPos = nullptr;
    QSlider *m_wmOpacity = nullptr;
    QLabel *m_wmOpacityLabel = nullptr;
    QLabel *m_wmPreview = nullptr;

    // 一般
    HotkeyEdit *m_hotkey = nullptr;
    QComboBox *m_language = nullptr;
    QCheckBox *m_autostart = nullptr;
    QCheckBox *m_notify = nullptr;
    QLineEdit *m_ffmpegPath = nullptr;
    QLabel *m_ffmpegStatus = nullptr;
};
