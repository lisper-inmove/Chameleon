#include "ui/filter_panel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

namespace chameleon {

FilterPanel::FilterPanel(QWidget *parent) : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("滤镜"), this);
    QFont font = title->font();
    font.setBold(true);
    title->setFont(font);

    auto *hint = new QLabel(QStringLiteral("滤镜功能待实现"), this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(hint);

    // Parameter control skeletons (disabled until the filter feature lands)
    const QStringList names{"亮度", "对比度", "饱和度"};
    for (const QString &name : names) {
        auto *row = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *label = new QLabel(name, row);
        label->setFixedWidth(56);
        auto *slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(-100, 100);
        slider->setValue(0);
        slider->setEnabled(false);
        rowLayout->addWidget(label);
        rowLayout->addWidget(slider);
        layout->addWidget(row);
    }
    layout->addStretch();
}

} // namespace chameleon
