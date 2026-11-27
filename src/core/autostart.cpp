#include "core/autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <windows.h>

namespace {
const char *kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char *kValue = "qScreenRec";
}

bool AutoStart::isEnabled()
{
    if (isPackaged())
        return false;
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return run.contains(QString::fromLatin1(kValue));
}

bool AutoStart::setEnabled(bool enabled)
{
    if (isPackaged())
        return false; // Store 版由 manifest 的 StartupTask 管理
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (enabled) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(QString::fromLatin1(kValue), QStringLiteral("\"%1\" --minimized").arg(exe));
    } else {
        run.remove(QString::fromLatin1(kValue));
    }
    run.sync();
    return run.status() == QSettings::NoError;
}

bool AutoStart::isPackaged()
{
    // GetCurrentPackageFullName 於 Windows 8 以上提供；未封裝時回傳 APPMODEL_ERROR_NO_PACKAGE
    using Fn = LONG(WINAPI *)(UINT32 *, PWSTR);
    static const Fn fn = reinterpret_cast<Fn>(
        reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetCurrentPackageFullName")));
    if (!fn)
        return false;
    UINT32 len = 0;
    const LONG rc = fn(&len, nullptr);
    return rc == ERROR_INSUFFICIENT_BUFFER;
}
