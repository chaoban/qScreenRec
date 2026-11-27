#pragma once

#include <QKeySequence>
#include <QLineEdit>

// 錄製單一快捷鍵組合的輸入框（需包含 Ctrl / Alt / Shift / Win 其中之一，F 鍵除外）
class HotkeyEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit HotkeyEdit(QWidget *parent = nullptr);
    QKeySequence keySequence() const { return m_seq; }
    void setKeySequence(const QKeySequence &seq);

signals:
    void keySequenceChanged(const QKeySequence &seq);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private:
    QKeySequence m_seq;
};
