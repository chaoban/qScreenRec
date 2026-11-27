#pragma once

#include "core/settings.h"

#include <QObject>
#include <QString>
#include <atomic>
#include <thread>

// 錄影引擎：在背景執行緒擷取畫面、合成游標與浮水印，再透過管線交給 ffmpeg 編碼
class Recorder : public QObject
{
    Q_OBJECT
public:
    enum class State { Idle, Recording, Finishing };
    Q_ENUM(State)
    enum class StopReason { User, TimeLimit, DiskFull };
    Q_ENUM(StopReason)

    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    bool start(const AppSettings &settings, const QString &ffmpegPath, const QString &encoderId, QString *error);
    void stop();

    State state() const { return m_state; }
    // 開始錄影時若原本的儲存資料夾無法使用，會改存到預設資料夾；此時回傳 true
    bool usedFallbackFolder() const { return m_usedFallback; }
    qint64 elapsedMs() const { return m_elapsedMs.load(); }
    QString currentFile() const { return m_file; }
    QString backendName() const;

signals:
    void stateChanged(Recorder::State state);
    // reason：使用者停止、達到錄影時間、磁碟空間不足
    void finished(const QString &file, bool ok, Recorder::StopReason reason, const QString &error);

private:
    struct Job
    {
        AppSettings settings;
        QString ffmpegPath;
        QString encoderId;
        QString file;
    };
    void run(Job job);
    void setState(State s);

    State m_state = State::Idle;
    QString m_file;
    bool m_usedFallback = false;
    std::thread m_thread;
    std::atomic_bool m_stop { false };
    std::atomic<qint64> m_elapsedMs { 0 };
    std::atomic<int> m_backend { 0 }; // 0 = 未知, 1 = DXGI, 2 = GDI
};
