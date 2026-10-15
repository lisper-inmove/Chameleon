#include "ui/gradient_dialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chameleon {

GradientDialog::GradientDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("梯度提取"));
    setObjectName(QStringLiteral("gradientDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->setObjectName(QStringLiteral("gradientTypeCombo"));
    m_typeCombo->addItem(QStringLiteral("Prewitt"));
    m_typeCombo->addItem(QStringLiteral("Sobel"));
    m_typeCombo->addItem(QStringLiteral("Scharr"));
    m_typeCombo->setCurrentIndex(1);  // Sobel

    m_dxSpin = new QSpinBox(this);
    m_dxSpin->setObjectName(QStringLiteral("gradientDxSpin"));
    m_dxSpin->setRange(0, 2);
    m_dxSpin->setValue(1);

    m_dySpin = new QSpinBox(this);
    m_dySpin->setObjectName(QStringLiteral("gradientDySpin"));
    m_dySpin->setRange(0, 2);
    m_dySpin->setValue(0);

    m_ksizeSpin = new QSpinBox(this);
    m_ksizeSpin->setObjectName(QStringLiteral("gradientKsizeSpin"));
    m_ksizeSpin->setRange(1, 31);
    m_ksizeSpin->setSingleStep(2);  // odd sizes
    m_ksizeSpin->setValue(3);

    m_scaleSpin = new QDoubleSpinBox(this);
    m_scaleSpin->setObjectName(QStringLiteral("gradientScaleSpin"));
    m_scaleSpin->setRange(0.0, 100.0);
    m_scaleSpin->setSingleStep(0.1);
    m_scaleSpin->setValue(1.0);

    m_deltaSpin = new QDoubleSpinBox(this);
    m_deltaSpin->setObjectName(QStringLiteral("gradientDeltaSpin"));
    m_deltaSpin->setRange(-255.0, 255.0);
    m_deltaSpin->setSingleStep(1.0);
    m_deltaSpin->setValue(0.0);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("gradientHint"));
    m_hint->setWordWrap(true);

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("gradientApplyButton"));
    connect(applyButton, &QPushButton::clicked,
            this, &GradientDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("类型:"), m_typeCombo);
    form->addRow(QStringLiteral("dx:"), m_dxSpin);
    form->addRow(QStringLiteral("dy:"), m_dySpin);
    form->addRow(QStringLiteral("核大小:"), m_ksizeSpin);
    form->addRow(QStringLiteral("scale:"), m_scaleSpin);
    form->addRow(QStringLiteral("delta:"), m_deltaSpin);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_hint);
    layout->addWidget(applyButton);
    resize(340, 280);

    connect(m_typeCombo, &QComboBox::currentIndexChanged,
            this, [this] { updateParameterVisibility(); });
    updateParameterVisibility();
}

void GradientDialog::updateParameterVisibility()
{
    switch (m_typeCombo->currentIndex()) {
    case 0:  // Prewitt
        m_ksizeSpin->setEnabled(false);
        m_hint->setText(QStringLiteral(
            "Prewitt 3×3 核(filter2D);dx/dy 为方向开关,同时开启则组合两方向。"));
        break;
    case 2:  // Scharr
        m_ksizeSpin->setEnabled(false);
        m_hint->setText(QStringLiteral(
            "Scharr 固定 3×3;仅支持单个方向的 1 阶导数,dx/dy 均设时取 dx。"));
        break;
    case 1:  // Sobel
    default:
        m_ksizeSpin->setEnabled(true);
        m_hint->setText(QStringLiteral(
            "Sobel 核大小可选(奇数);dx/dy 为求导阶数(受核大小限制)。"));
        break;
    }
}

GradientParams GradientDialog::gradientParams() const
{
    GradientParams params;
    switch (m_typeCombo->currentIndex()) {
    case 0:
        params.type = GradientType::Prewitt;
        break;
    case 2:
        params.type = GradientType::Scharr;
        break;
    case 1:
    default:
        params.type = GradientType::Sobel;
        break;
    }
    params.dx = m_dxSpin->value();
    params.dy = m_dySpin->value();
    params.ksize = m_ksizeSpin->value();
    params.scale = m_scaleSpin->value();
    params.delta = m_deltaSpin->value();
    return params;
}

QString GradientDialog::currentTypeName() const
{
    return m_typeCombo->currentText();
}

} // namespace chameleon
