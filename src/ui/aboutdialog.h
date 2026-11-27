#pragma once

#include <QDialog>

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    AboutDialog(const QString &language, const QString &ffmpegVersion, QWidget *parent = nullptr);

private:
    void showLicenses();
    QString m_language;
};
