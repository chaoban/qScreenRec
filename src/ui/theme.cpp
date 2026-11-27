#include "ui/theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QWidget>
#include <dwmapi.h>

void Theme::apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    const QColor bg(0x1F, 0x20, 0x23);
    const QColor base(0x27, 0x28, 0x2C);
    const QColor text(0xE8, 0xE8, 0xEA);
    const QColor accent(kAccent);
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, bg);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(0x2E, 0x2F, 0x34));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::LinkVisited, accent);
    p.setColor(QPalette::PlaceholderText, QColor(0x8A, 0x8B, 0x90));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6A, 0x6B, 0x70));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6A, 0x6B, 0x70));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6A, 0x6B, 0x70));
    app.setPalette(p);

    QFont f(QStringLiteral("Microsoft JhengHei UI"), 10);
    app.setFont(f);

    app.setStyleSheet(QStringLiteral(R"(
        QPushButton {
            background: #2E2F34; color: #E8E8EA; border: 1px solid #45464C;
            border-radius: 6px; padding: 6px 16px; min-height: 22px;
        }
        QPushButton:hover { background: #393A40; }
        QPushButton:pressed { background: #26272B; }
        QPushButton:disabled { color: #6A6B70; }
        QPushButton[accent="true"] { background: %1; border-color: %1; color: #FFFFFF; font-weight: bold; }
        QPushButton[accent="true"]:hover { background: #F59B3E; }
        QGroupBox {
            border: 1px solid #3A3B40; border-radius: 8px; margin-top: 14px; padding: 10px 8px 8px 8px;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: %1; font-weight: bold; }
        QComboBox, QLineEdit, QSpinBox, QKeySequenceEdit {
            background: #27282C; border: 1px solid #45464C; border-radius: 5px; padding: 4px 6px; min-height: 20px;
        }
        QComboBox:focus, QLineEdit:focus, QSpinBox:focus { border-color: %1; }
        QComboBox QAbstractItemView { background: #27282C; selection-background-color: %1; }
        QTabWidget::pane { border: 1px solid #3A3B40; border-radius: 6px; top: -1px; }
        QTabBar::tab { background: #26272B; padding: 7px 18px; border: 1px solid #3A3B40; border-bottom: none;
                       border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }
        QTabBar::tab:selected { background: #1F2023; color: %1; }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked { background: %1; border: 1px solid %1; border-radius: 3px; }
        QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #5A5B60; border-radius: 3px; background: #27282C; }
        QMenu { background: #27282C; border: 1px solid #3A3B40; padding: 4px; }
        QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }
        QMenu::item:selected { background: %1; color: white; }
        QMenu::item:disabled { color: #8A8B90; }
        QMenu::separator { height: 1px; background: #3A3B40; margin: 4px 8px; }
        QTextBrowser { background: #1F2023; border: none; }
        QSlider::groove:horizontal { height: 4px; background: #45464C; border-radius: 2px; }
        QSlider::handle:horizontal { background: %1; width: 14px; margin: -6px 0; border-radius: 7px; }
        QSlider::sub-page:horizontal { background: %1; border-radius: 2px; }
    )").arg(QLatin1String(kAccent)));
}

void Theme::darkTitleBar(QWidget *w)
{
    if (!w)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(w->winId());
    const BOOL dark = TRUE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}
