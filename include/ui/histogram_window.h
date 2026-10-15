#pragma once

#include <array>

#include <QColor>
#include <QWidget>

namespace chameleon {

// Top-level popup that draws a 256-bin channel histogram. One window per
// channel; closes and deletes itself on user close.
class HistogramWindow : public QWidget
{
    Q_OBJECT
public:
    HistogramWindow(const QString &title, const QColor &barColor,
                    const std::array<double, 256> &bins,
                    QWidget *parent = nullptr);

    const std::array<double, 256> &bins() const { return m_bins; }
    // Updates the plotted data in place (window reuse)
    void setBins(const std::array<double, 256> &bins);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_barColor;
    std::array<double, 256> m_bins;
};

} // namespace chameleon
