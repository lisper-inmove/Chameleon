#include "ui/filter_panel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace chameleon {

namespace {

QSlider *makeSlider(QWidget *parent, const char *objectName)
{
    auto *slider = new QSlider(Qt::Horizontal, parent);
    slider->setObjectName(QString::fromUtf8(objectName));
    slider->setRange(-100, 100);
    slider->setValue(0);
    return slider;
}

void addSliderRow(QVBoxLayout *layout, const QString &labelText, QSlider *slider)
{
    auto *rowWidget = new QWidget(layout->parentWidget());
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    auto *label = new QLabel(labelText, rowWidget);
    label->setFixedWidth(56);
    rowLayout->addWidget(label);
    rowLayout->addWidget(slider);
    layout->addWidget(rowWidget);
}

} // namespace

FilterPanel::FilterPanel(QWidget *parent) : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("滤镜"), this);
    QFont font = title->font();
    font.setBold(true);
    title->setFont(font);

    m_brightness = makeSlider(this, "brightnessSlider");
    m_contrast = makeSlider(this, "contrastSlider");
    m_saturation = makeSlider(this, "saturationSlider");

    connect(m_brightness, &QSlider::valueChanged, this, &FilterPanel::emitParams);
    connect(m_contrast, &QSlider::valueChanged, this, &FilterPanel::emitParams);
    connect(m_saturation, &QSlider::valueChanged, this, &FilterPanel::emitParams);

    auto *buttons = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);

    auto *reset = new QPushButton(QStringLiteral("重置"), buttons);
    reset->setObjectName("resetButton");
    connect(reset, &QPushButton::clicked, this, [this] {
        resetSliders();
        emit resetClicked();
    });

    auto *apply = new QPushButton(QStringLiteral("应用"), buttons);
    apply->setObjectName("applyButton");
    connect(apply, &QPushButton::clicked, this, &FilterPanel::applyClicked);

    buttonLayout->addWidget(reset);
    buttonLayout->addWidget(apply);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    addSliderRow(layout, QStringLiteral("亮度"), m_brightness);
    addSliderRow(layout, QStringLiteral("对比度"), m_contrast);
    addSliderRow(layout, QStringLiteral("饱和度"), m_saturation);
    layout->addWidget(buttons);
    layout->addStretch();
}

void FilterPanel::emitParams()
{
    emit filterParamsChanged(m_brightness->value(), m_contrast->value(),
                             m_saturation->value());
}

void FilterPanel::resetSliders()
{
    const QSignalBlocker blockBrightness(m_brightness);
    const QSignalBlocker blockContrast(m_contrast);
    const QSignalBlocker blockSaturation(m_saturation);
    m_brightness->setValue(0);
    m_contrast->setValue(0);
    m_saturation->setValue(0);
}

} // namespace chameleon
