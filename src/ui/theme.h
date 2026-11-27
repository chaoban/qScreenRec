#pragma once

class QApplication;
class QWidget;

namespace Theme {
constexpr const char *kAccent = "#F08A24";
void apply(QApplication &app);
// 讓視窗標題列也使用深色（Windows 10 20H1 以上 / Windows 11）
void darkTitleBar(QWidget *w);
}
