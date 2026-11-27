#pragma once

#include <QSize>
#include <QString>
#include <QStringList>
#include <atomic>

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
    // cancel 可為 nullptr；設為 true 時會盡快中止偵測
    static QList<EncoderInfo> probeEncoders(const QString &ffmpegPath, const std::atomic_bool *cancel = nullptr);
    static QList<EncoderInfo> knownEncoders();
    static QString displayName(const QString &encoderId);

    static QSize outputSize(const QSize &source, int targetHeight);

    // 輸入為 stdin 的 bgr0 rawvideo，輸出 MP4 / MKV（GIF 先錄成 MKV，再用下列兩步轉檔）
    static QStringList buildArgs(const AppSettings &s, const QString &encoderId,
                                 const QSize &source, int fps, const QString &outputFile);
    // GIF 兩階段轉檔：先產生調色盤，再套用。不會把整段影片暫存在記憶體
    static QStringList gifPaletteArgs(const QString &input, int fps, const QString &palette);
    static QStringList gifEncodeArgs(const QString &input, const QString &palette, int fps, const QString &output);
};
