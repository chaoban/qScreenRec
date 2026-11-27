#include "core/watermark.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

QImage Watermark::render(const QString &text, int frameHeight, int opacityPercent)
{
    const QString t = text.trimmed().left(AppSettings::kWatermarkMaxChars);
    if (t.isEmpty())
        return {};

    QFont font(QStringLiteral("Microsoft JhengHei UI"));
    font.setStyleHint(QFont::SansSerif);
    font.setBold(true);
    font.setPixelSize(qMax(14, frameHeight / 28));

    const QFontMetrics fm(font);
    const int outline = qMax(2, font.pixelSize() / 10);
    const QSize size(fm.horizontalAdvance(t) + outline * 4, fm.height() + outline * 4);

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setOpacity(qBound(10, opacityPercent, 100) / 100.0);

    QPainterPath path;
    path.addText(outline * 2, outline * 2 + fm.ascent(), font, t);
    p.setPen(QPen(QColor(0, 0, 0, 200), outline * 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawPath(path);
    p.end();
    return img;
}

void Watermark::configure(const QString &text, WatermarkPosition pos, int opacityPercent, const QSize &frameSize)
{
    m_layer = render(text, frameSize.height(), opacityPercent);
    if (m_layer.isNull())
        return;
    const int margin = qMax(8, frameSize.height() / 40);
    const int w = m_layer.width();
    const int h = m_layer.height();
    switch (pos) {
    case WatermarkPosition::TopLeft: m_pos = QPoint(margin, margin); break;
    case WatermarkPosition::TopRight: m_pos = QPoint(frameSize.width() - w - margin, margin); break;
    case WatermarkPosition::BottomLeft: m_pos = QPoint(margin, frameSize.height() - h - margin); break;
    case WatermarkPosition::Center: m_pos = QPoint((frameSize.width() - w) / 2, (frameSize.height() - h) / 2); break;
    case WatermarkPosition::BottomRight:
    default: m_pos = QPoint(frameSize.width() - w - margin, frameSize.height() - h - margin); break;
    }
}

void Watermark::paint(QPainter &p) const
{
    if (!m_layer.isNull())
        p.drawImage(m_pos, m_layer);
}
