#include "ui/sharpen_dialog.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chameleon {

SharpenDialog::SharpenDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("拉普拉斯锐化"));
    setObjectName(QStringLiteral("sharpenDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_ksizeSpin = new QSpinBox(this);
    m_ksizeSpin->setObjectName(QStringLiteral("sharpenKsizeSpin"));
    m_ksizeSpin->setRange(1, 7);
    m_ksizeSpin->setSingleStep(2);  // 1 / 3 / 5 / 7
    m_ksizeSpin->setValue(3);

    m_strengthSpin = new QDoubleSpinBox(this);
    m_strengthSpin->setObjectName(QStringLiteral("sharpenStrengthSpin"));
    m_strengthSpin->setRange(0.0, 5.0);
    m_strengthSpin->setSingleStep(0.1);
    m_strengthSpin->setValue(1.0);

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("sharpenApplyButton"));
    connect(applyButton, &QPushButton::clicked,
            this, &SharpenDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("核大小:"), m_ksizeSpin);
    form->addRow(QStringLiteral("强度:"), m_strengthSpin);
    form->addRow(QString(), applyButton);

    // Sharpness readout of the current image (or selection)
    m_sharpnessLabel = new QLabel(this);
    m_sharpnessLabel->setObjectName(QStringLiteral("sharpenSharpnessLabel"));
    m_sharpnessLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *measureButton = new QPushButton(QStringLiteral("测量锐度"), this);
    measureButton->setObjectName(QStringLiteral("sharpenMeasureButton"));
    connect(measureButton, &QPushButton::clicked,
            this, &SharpenDialog::measureRequested);

    auto *sharpnessGroup = new QGroupBox(QStringLiteral("锐度"), this);
    auto *sharpnessLayout = new QVBoxLayout(sharpnessGroup);
    sharpnessLayout->addWidget(m_sharpnessLabel);
    sharpnessLayout->addWidget(measureButton);

    auto *hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setText(QStringLiteral(
        "锐化:out = 原图 − 强度 × 拉普拉斯,边缘处产生过冲而显得更锐利。"
        "锐度为拉普拉斯方差:数值越大越清晰,可用于比较锐化前后的效果。"));

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(sharpnessGroup);
    layout->addWidget(hint);
    resize(380, 300);

    setSharpness(0.0);
}

SharpenParams SharpenDialog::sharpenParams() const
{
    SharpenParams params;
    params.ksize = m_ksizeSpin->value();
    params.strength = m_strengthSpin->value();
    return params;
}

void SharpenDialog::setSharpness(double variance)
{
    m_sharpnessLabel->setText(
        QStringLiteral("锐度(拉普拉斯方差):%1").arg(QString::number(variance, 'f', 2)));
}

} // namespace chameleon
