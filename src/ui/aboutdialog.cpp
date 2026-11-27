#include "ui/aboutdialog.h"

#include "core/license.h"
#include "ui/theme.h"
#include "version.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
QString link(const QString &url, const QString &text = QString())
{
    return QStringLiteral("<a style='color:%1' href='%2'>%3</a>")
        .arg(QLatin1String(Theme::kAccent), url, text.isEmpty() ? url : text);
}
} // namespace

AboutDialog::AboutDialog(const QString &language, const QString &ffmpegVersion, QWidget *parent)
    : QDialog(parent), m_language(language)
{
    setWindowTitle(tr("About %1").arg(QStringLiteral(APP_NAME)));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setAttribute(Qt::WA_DeleteOnClose);

    // SAISAI 商標（白底圓角）
    auto *logo = new QLabel;
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(QStringLiteral(":/icons/saisai_logo.png"))
                        .scaledToWidth(380, Qt::SmoothTransformation));
    logo->setStyleSheet(QStringLiteral("background: white; border-radius: 12px; padding: 14px 24px;"));
    auto *logoRow = new QHBoxLayout;
    logoRow->addStretch();
    logoRow->addWidget(logo);
    logoRow->addStretch();

    // 軟體圖示與名稱
    auto *icon = new QLabel;
    icon->setPixmap(QPixmap(QStringLiteral(":/icons/app.png")).scaled(84, 84, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto *name = new QLabel(QStringLiteral(APP_NAME));
    name->setStyleSheet(QStringLiteral("font-size: 24pt;"));
    auto *ver = new QLabel(tr("Version %1").arg(QStringLiteral(APP_VERSION)) + QStringLiteral(" · ") + License::editionName());
    auto *desc = new QLabel(tr("Simple screen recorder"));
    auto *nameCol = new QVBoxLayout;
    nameCol->setSpacing(2);
    nameCol->addWidget(name);
    nameCol->addWidget(ver);
    nameCol->addWidget(desc);
    auto *head = new QHBoxLayout;
    head->setSpacing(24);
    head->addWidget(icon, 0, Qt::AlignTop);
    head->addLayout(nameCol, 1);

    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color: #45464C;"));

    auto *copyright = new QLabel(tr("%1 All rights reserved.").arg(QStringLiteral(APP_COPYRIGHT)));
    copyright->setStyleSheet(QStringLiteral("font-weight: bold;"));

    const QString email = QStringLiteral(APP_EMAIL);
    auto *contact = new QLabel(tr("Contact: ") + link(QStringLiteral("mailto:") + email, email));
    contact->setOpenExternalLinks(true);

    const QString ffVer = ffmpegVersion.isEmpty() ? tr("(not found)") : ffmpegVersion;
    const QString legal =
        tr("This software is built with Qt %1; Qt is used under the GNU LGPL v3.").arg(QString::fromLatin1(qVersion()))
        + QStringLiteral("<br>") + tr("Qt source code: ") + link(QStringLiteral("https://www.qt.io/download-open-source"))
        + QStringLiteral("<br>") + tr("Video encoding uses FFmpeg %1 under the GNU LGPL v3.").arg(ffVer)
        + QStringLiteral("<br>") + tr("FFmpeg source code: ") + link(QStringLiteral("https://ffmpeg.org/download.html"))
        + QStringLiteral("<br>") + tr("See \"Licenses\" for other third-party components and full license texts.");
    auto *legalLabel = new QLabel(legal);
    legalLabel->setOpenExternalLinks(true);
    legalLabel->setWordWrap(true);

    auto *licBtn = new QPushButton(tr("Licenses…"));
    auto *qtBtn = new QPushButton(tr("About Qt…"));
    auto *closeBtn = new QPushButton(tr("Close"));
    closeBtn->setProperty("accent", true);
    closeBtn->setDefault(true);
    connect(licBtn, &QPushButton::clicked, this, &AboutDialog::showLicenses);
    connect(qtBtn, &QPushButton::clicked, this, [this] { QMessageBox::aboutQt(this); });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(licBtn);
    btnRow->addWidget(qtBtn);
    btnRow->addWidget(closeBtn);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(36, 28, 36, 24);
    layout->setSpacing(14);
    layout->addLayout(logoRow);
    layout->addSpacing(6);
    layout->addLayout(head);
    layout->addWidget(line);
    layout->addWidget(copyright);
    layout->addWidget(contact);
    layout->addWidget(legalLabel);
    layout->addSpacing(8);
    layout->addLayout(btnRow);
    setFixedWidth(620);
    Theme::darkTitleBar(this);
}

void AboutDialog::showLicenses()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Licenses"));
    dlg.resize(680, 560);
    auto *browser = new QTextBrowser(&dlg);
    browser->setOpenExternalLinks(true);
    QFile f(QStringLiteral(":/help/licenses_%1.html").arg(m_language == QLatin1String("zh_TW") ? m_language : QStringLiteral("en")));
    if (f.open(QIODevice::ReadOnly))
        browser->setHtml(QString::fromUtf8(f.readAll()));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Close"));
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto *l = new QVBoxLayout(&dlg);
    l->addWidget(browser);
    l->addWidget(buttons);
    Theme::darkTitleBar(&dlg);
    dlg.exec();
}
