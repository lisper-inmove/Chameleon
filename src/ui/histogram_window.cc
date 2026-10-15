#include "ui/histogram_window.h"

#include <algorithm>

#include <QPainter>
#include <QPolygonF>

namespace chameleon {

namespace {
constexpr int kMarginLeft = 8;
constexpr int kMarginRight = 8;
constexpr int kMarginTop = 8;
constexpr int kMarginBottom = 18;
} // namespace

HistogramWindow::HistogramWindow(const QString &title, const QColor &barColor,
                                 const std::array<double, 256> &bins,
                                 QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_barColor(barColor)
    , m_bins(bins)
{
    setWindowTitle(title);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(360, 240);
}

void HistogramWindow::setBins(const std::array<double, 256> &bins)
{
    m_bins = bins;
    update();
}

void HistogramWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window());

    const QRectF plot(kMarginLeft, kMarginTop,
                      width() - kMarginLeft - kMarginRight,
                      height() - kMarginTop - kMarginBottom);
    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.drawRect(plot);

    const double maxCount = std::max(1.0, *std::max_element(m_bins.begin(), m_bins.end()));

    // Filled area under the curve: one x step per bin
    QPolygonF area;
    area << QPointF(plot.left(), plot.bottom());
    for (int i = 0; i < 256; ++i) {
        const double x = plot.left() + plot.width() * i / 255.0;
        const double y = plot.bottom() - plot.height() * (m_bins[i] / maxCount);
        area << QPointF(x, y);
    }
    area << QPointF(plot.right(), plot.bottom());

    QColor fill = m_barColor;
    fill.setAlpha(170);
    p.setPen(QPen(m_barColor, 1));
    p.setBrush(fill);
    p.drawPolygon(area);

    // Axis labels: 0 / 64 / 128 / 192 / 255 under the plot
    p.setPen(palette().color(QPalette::Text));
    QFont labelFont = font();
    labelFont.setPointSizeF(std::max(7.0, labelFont.pointSizeF() * 0.8));
    p.setFont(labelFont);
    const int ticks[] = {0, 64, 128, 192, 255};
    for (int tick : ticks) {
        const QString text = QString::number(tick);
        const double x = plot.left() + plot.width() * tick / 255.0;
        const double textWidth = QFontMetricsF(labelFont).horizontalAdvance(text);
        double textX = x - textWidth / 2.0;
        textX = std::clamp(textX, 0.0, width() - textWidth);
        p.drawText(QPointF(textX, height() - 5), text);
    }
}

} // namespace chameleon
