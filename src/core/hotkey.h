#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>

// 以 Win32 RegisterHotKey 註冊的全域快捷鍵（程式在背景時也有效）
class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    // 回傳 false 代表按鍵組合無效，或已被其他程式佔用
    bool setShortcut(const QKeySequence &seq);
    void unregister();
    bool isRegistered() const { return m_registered; }

    // 將 Qt 按鍵轉為 Win32 virtual-key 與修飾鍵；不支援時回傳 false
    static bool toNative(const QKeySequence &seq, unsigned int &mods, unsigned int &vk);

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void activated();

private:
    int m_id;
    bool m_registered = false;
};
