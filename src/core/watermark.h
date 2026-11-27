#pragma once

#include "core/settings.h"
#include <QImage>
#include <QPoint>

// 預先把浮水印文字繪成透明圖層，每一格只需貼上，降低錄影時的 CPU 負擔
class Watermark
{
public:
    void configure(const QString &text, WatermarkPosition pos, int opacityPercent, const QSize &frameSize);
    bool isEmpty() const { return m_layer.isNull(); }
    void paint(QPainter &p) const;
    const QImage &layer() const { return m_layer; }

    // 提供設定視窗預覽使用
    static QImage render(const QString &text, int frameHeight, int opacityPercent);

private:
    QImage m_layer;
    QPoint m_pos;
};
