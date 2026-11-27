#pragma once

#include <QDialog>

class QTextBrowser;

// 使用說明（F1）
class HelpDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HelpDialog(QWidget *parent = nullptr);
    void load(const QString &language, const QString &hotkeyText);

private:
    QTextBrowser *m_browser;
};
