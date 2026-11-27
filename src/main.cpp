#include "app.h"
#include "core/settings.h"
#include "ui/theme.h"
#include "version.h"

#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral(APP_NAME));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setOrganizationName(QStringLiteral("SAISAI"));
    QApplication::setOrganizationDomain(QStringLiteral("saisai.dev"));
    QApplication::setQuitOnLastWindowClosed(false); // 常駐於系統匣
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    Theme::apply(app);

    // 只允許執行一份（避免快捷鍵衝突）
    QLockFile lock(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                       .filePath(QStringLiteral("qScreenRec.lock")));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100)) {
        const bool zh = AppSettings::load().effectiveLanguage() == QLatin1String("zh_TW");
        QMessageBox::information(nullptr, QStringLiteral(APP_NAME),
                                 zh ? QStringLiteral("qScreenRec 已在執行中，請在系統匣（右下角）找到它的圖示。")
                                    : QStringLiteral("qScreenRec is already running. Look for its icon in the system tray."));
        return 0;
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, QStringLiteral(APP_NAME),
                              QApplication::translate("main", "The system tray is not available."));
        return 1;
    }

    const bool minimized = app.arguments().contains(QStringLiteral("--minimized"));
    App controller(minimized);
    return app.exec();
}
