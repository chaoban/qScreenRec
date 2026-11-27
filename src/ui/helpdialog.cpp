#include "ui/helpdialog.h"

#include "ui/theme.h"
#include "version.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent)
{
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    resize(760, 680);
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    auto *l = new QVBoxLayout(this);
    l->addWidget(m_browser);
    l->addWidget(buttons);
    Theme::darkTitleBar(this);
}

void HelpDialog::load(const QString &language, const QString &hotkeyText)
{
    const QString lang = language == QLatin1String("zh_TW") ? language : QStringLiteral("en");
    setWindowTitle(tr("%1 Help").arg(QStringLiteral(APP_NAME)));
    if (auto *box = findChild<QDialogButtonBox *>())
        box->button(QDialogButtonBox::Close)->setText(tr("Close"));

    QFile f(QStringLiteral(":/help/help_%1.html").arg(lang));
    QString html;
    if (f.open(QIODevice::ReadOnly))
        html = QString::fromUtf8(f.readAll());
    html.replace(QStringLiteral("{{HOTKEY}}"), hotkeyText.toHtmlEscaped());
    html.replace(QStringLiteral("{{VERSION}}"), QStringLiteral(APP_VERSION));
    html.replace(QStringLiteral("{{ACCENT}}"), QLatin1String(Theme::kAccent));
    m_browser->setHtml(html);
}
