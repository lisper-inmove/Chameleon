#include "ui/edge_dialog.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chameleon {

EdgeDialog::EdgeDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("边缘检测(Canny)"));
    setObjectName(QStringLiteral("edgeDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_threshold1Spin = new QSpinBox(this);
    m_threshold1Spin->setObjectName(QStringLiteral("edgeThreshold1Spin"));
    m_threshold1Spin->setRange(0, 1000);
    m_threshold1Spin->setValue(100);

    m_threshold2Spin = new QSpinBox(this);
    m_threshold2Spin->setObjectName(QStringLiteral("edgeThreshold2Spin"));
    m_threshold2Spin->setRange(0, 1000);
    m_threshold2Spin->setValue(200);

    m_apertureSpin = new QSpinBox(this);
    m_apertureSpin->setObjectName(QStringLiteral("edgeApertureSpin"));
    m_apertureSpin->setRange(3, 7);
    m_apertureSpin->setSingleStep(2);  // 3 / 5 / 7
    m_apertureSpin->setValue(3);

    m_l2GradientCheck = new QCheckBox(QStringLiteral("L2 梯度"), this);
    m_l2GradientCheck->setObjectName(QStringLiteral("edgeL2GradientCheck"));
    m_l2GradientCheck->setChecked(false);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("edgeHint"));
    m_hint->setWordWrap(true);
    m_hint->setText(QStringLiteral(
        "双阈值(hysteresis):低于阈值1的梯度被丢弃,高于阈值2的被保留,之间仅在与强边"
        "相连时保留;系数越小边缘越多。核大小为内部 Sobel 孔径(3–7)。"));

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("edgeApplyButton"));
    connect(applyButton, &QPushButton::clicked,
            this, &EdgeDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("threshold1:"), m_threshold1Spin);
    form->addRow(QStringLiteral("threshold2:"), m_threshold2Spin);
    form->addRow(QStringLiteral("核大小:"), m_apertureSpin);
    form->addRow(QString(), m_l2GradientCheck);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_hint);
    layout->addWidget(applyButton);
    resize(380, 260);
}

CannyParams EdgeDialog::cannyParams() const
{
    CannyParams params;
    params.threshold1 = m_threshold1Spin->value();
    params.threshold2 = m_threshold2Spin->value();
    params.apertureSize = m_apertureSpin->value();
    params.l2Gradient = m_l2GradientCheck->isChecked();
    return params;
}

} // namespace chameleon
