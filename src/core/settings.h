#pragma once

#include <QString>
#include <QKeySequence>

// 錄影輸出格式
enum class VideoFormat { Mp4 = 0, Mkv = 1, Gif = 2 };

// 輸出解析度（依高度縮放，保持長寬比；不會放大）
enum class Resolution { Original = 0, P2160 = 1, P1440 = 2, P1080 = 3, P720 = 4, P480 = 5 };

enum class Quality { High = 0, Medium = 1, Low = 2 };

enum class WatermarkPosition { TopLeft = 0, TopRight = 1, BottomLeft = 2, BottomRight = 3, Center = 4 };

struct AppSettings
{
    // 錄影
    int monitorIndex = 0;                 // DXGI 輸出索引
    VideoFormat format = VideoFormat::Mp4;
    Resolution resolution = Resolution::Original;
    int fps = 30;
    Quality quality = Quality::High;
    bool drawCursor = true;
    bool highlightClicks = false;         // 滑鼠點擊時加上光圈
    int durationLimitMin = 0;             // 0 = 不限
    int countdownSec = 3;                 // 開始前倒數（0 = 不倒數）
    QString outputDir;
    QString encoder;                      // 空字串 = 自動

    // 浮水印
    bool watermarkEnabled = false;
    QString watermarkText;                // 最多 20 字
    WatermarkPosition watermarkPos = WatermarkPosition::BottomRight;
    int watermarkOpacity = 70;            // 10–100 %

    // 一般
    QKeySequence hotkey = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R);
    QString language;                     // "zh_TW" / "en"；空 = 依系統
    bool autostart = false;
    bool notifyOnSave = true;
    QString ffmpegPath;                   // 空 = 自動尋找

    static constexpr int kWatermarkMaxChars = 20;

    static AppSettings load();
    void save() const;

    static QString defaultOutputDir();
    static QString systemLanguage();
    QString effectiveLanguage() const;

    QString fileExtension() const;
    int targetHeight() const;             // 0 = 原始
};
