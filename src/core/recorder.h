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

    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    bool start(const AppSettings &settings, const QString &ffmpegPath, const QString &encoderId, QString *error);
    void stop();

    State state() const { return m_state; }
    qint64 elapsedMs() const { return m_elapsedMs.load(); }
    QString currentFile() const { return m_file; }
    QString backendName() const;

signals:
    void stateChanged(Recorder::State state);
    // limitReached：因達到設定的錄影時間而自動停止
    void finished(const QString &file, bool ok, bool limitReached, const QString &error);

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
    std::thread m_thread;
    std::atomic_bool m_stop { false };
    std::atomic<qint64> m_elapsedMs { 0 };
    std::atomic<int> m_backend { 0 }; // 0 = 未知, 1 = DXGI, 2 = GDI
};
