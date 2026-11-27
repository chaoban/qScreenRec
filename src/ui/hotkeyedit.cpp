#include "ui/hotkeyedit.h"

#include <QKeyEvent>

HotkeyEdit::HotkeyEdit(QWidget *parent) : QLineEdit(parent)
{
    setReadOnly(true);
    setAlignment(Qt::AlignCenter);
}

void HotkeyEdit::setKeySequence(const QKeySequence &seq)
{
    m_seq = seq;
    setText(seq.toString(QKeySequence::NativeText));
}

void HotkeyEdit::focusInEvent(QFocusEvent *e)
{
    QLineEdit::focusInEvent(e);
    setPlaceholderText(tr("Press a key combination…"));
}

void HotkeyEdit::focusOutEvent(QFocusEvent *e)
{
    QLineEdit::focusOutEvent(e);
    setPlaceholderText(QString());
}

void HotkeyEdit::keyPressEvent(QKeyEvent *e)
{
    const int key = e->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta
        || key == Qt::Key_unknown || key == 0) {
        return;
    }
    if ((key == Qt::Key_Escape || key == Qt::Key_Tab) && e->modifiers() == Qt::NoModifier) {
        QLineEdit::keyPressEvent(e);
        return;
    }
    const Qt::KeyboardModifiers mods = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    const bool isFKey = key >= Qt::Key_F2 && key <= Qt::Key_F24; // F1 保留給說明
    if (mods == Qt::NoModifier && !isFKey)
        return;
    setKeySequence(QKeySequence(QKeyCombination(mods, static_cast<Qt::Key>(key))));
    emit keySequenceChanged(m_seq);
}
