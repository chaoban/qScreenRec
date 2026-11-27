#include "core/hotkey.h"

#include <QCoreApplication>
#include <windows.h>

namespace {
int g_nextId = 0xB000;

unsigned int keyToVk(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return 'A' + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return '0' + (key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return VK_F1 + (key - Qt::Key_F1);
    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_Print: return VK_SNAPSHOT;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_Minus: return VK_OEM_MINUS;
    case Qt::Key_Equal: return VK_OEM_PLUS;
    case Qt::Key_Comma: return VK_OEM_COMMA;
    case Qt::Key_Period: return VK_OEM_PERIOD;
    case Qt::Key_Slash: return VK_OEM_2;
    case Qt::Key_QuoteLeft: return VK_OEM_3;
    case Qt::Key_BracketLeft: return VK_OEM_4;
    case Qt::Key_Backslash: return VK_OEM_5;
    case Qt::Key_BracketRight: return VK_OEM_6;
    case Qt::Key_Apostrophe: return VK_OEM_7;
    case Qt::Key_Semicolon: return VK_OEM_1;
    default: return 0;
    }
}
} // namespace

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent), m_id(g_nextId++)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

GlobalHotkey::~GlobalHotkey()
{
    unregister();
    if (auto *app = QCoreApplication::instance())
        app->removeNativeEventFilter(this);
}

bool GlobalHotkey::toNative(const QKeySequence &seq, unsigned int &mods, unsigned int &vk)
{
    if (seq.isEmpty())
        return false;
    const QKeyCombination combo = seq[0];
    const Qt::KeyboardModifiers m = combo.keyboardModifiers();
    mods = MOD_NOREPEAT;
    if (m & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (m & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (m & Qt::AltModifier) mods |= MOD_ALT;
    if (m & Qt::MetaModifier) mods |= MOD_WIN;
    vk = keyToVk(combo.key());
    return vk != 0;
}

bool GlobalHotkey::setShortcut(const QKeySequence &seq)
{
    unregister();
    unsigned int mods = 0, vk = 0;
    if (!toNative(seq, mods, vk))
        return false;
    m_registered = RegisterHotKey(nullptr, m_id, mods, vk) != 0;
    return m_registered;
}

void GlobalHotkey::unregister()
{
    if (m_registered) {
        UnregisterHotKey(nullptr, m_id);
        m_registered = false;
    }
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
    if (eventType != "windows_generic_MSG")
        return false;
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == m_id) {
        emit activated();
        return true;
    }
    return false;
}
