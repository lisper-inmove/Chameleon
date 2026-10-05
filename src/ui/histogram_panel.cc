#include "ui/histogram_panel.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

namespace chameleon {

HistogramPanel::HistogramPanel(QWidget *parent) : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("直方图"), this);
    QFont font = title->font();
    font.setBold(true);
    title->setFont(font);

    auto *placeholder = new QFrame(this);
    placeholder->setFrameShape(QFrame::StyledPanel);
    placeholder->setMinimumHeight(140);
    auto *placeholderLayout = new QVBoxLayout(placeholder);
    placeholderLayout->addWidget(new QLabel(QStringLiteral("直方图待实现"), placeholder),
                                 0, Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(placeholder);
    layout->addStretch();
}

} // namespace chameleon
