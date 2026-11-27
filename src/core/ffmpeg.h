#pragma once

#include <QSize>
#include <QString>
#include <QStringList>

struct AppSettings;

struct EncoderInfo
{
    QString id;           // 例如 h264_nvenc
    QString displayName;
    bool hardware = false;
};

// 尋找 ffmpeg.exe、偵測可用的 H.264 編碼器、組出錄影參數
class FFmpeg
{
public:
    static QString locate(const QString &preferredPath);
    static QString version(const QString &ffmpegPath);           // 例如 "n8.1.3"
    // 實際試編碼，回傳能用的編碼器（依建議順序）。會花 1~5 秒，請在背景執行
    static QList<EncoderInfo> probeEncoders(const QString &ffmpegPath);
    static QList<EncoderInfo> knownEncoders();
    static QString displayName(const QString &encoderId);

    static QSize outputSize(const QSize &source, int targetHeight);

    // 輸入為 stdin 的 bgr0 rawvideo
    static QStringList buildArgs(const AppSettings &s, const QString &encoderId,
                                 const QSize &source, int fps, const QString &outputFile);
};
