#include "ui/convolution_dialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chameleon {

namespace {

QDoubleSpinBox *makeSpin(QWidget *parent, const char *name, double min, double max,  // NOLINT(bugprone-easily-swappable-parameters) - fixed argument order, few call sites
                         double value, double step)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setObjectName(QString::fromUtf8(name));
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setSingleStep(step);
    return spin;
}

} // namespace

ConvolutionDialog::ConvolutionDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("卷积 / 模糊"));
    setObjectName(QStringLiteral("convolutionDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->setObjectName(QStringLiteral("convolutionTypeCombo"));
    m_typeCombo->addItem(QStringLiteral("均值模糊"));
    m_typeCombo->addItem(QStringLiteral("高斯模糊"));
    m_typeCombo->addItem(QStringLiteral("中值模糊"));
    m_typeCombo->addItem(QStringLiteral("自定义卷积"));
    m_typeCombo->setCurrentIndex(1);  // Gaussian

    m_kernelSpin = new QSpinBox(this);
    m_kernelSpin->setObjectName(QStringLiteral("convolutionKernelSpin"));
    m_kernelSpin->setRange(1, 31);
    m_kernelSpin->setSingleStep(2);  // odd sizes
    m_kernelSpin->setValue(5);

    m_sigmaXSpin = makeSpin(this, "convolutionSigmaXSpin", 0.0, 20.0, 0.0, 0.5);
    m_sigmaYSpin = makeSpin(this, "convolutionSigmaYSpin", 0.0, 20.0, 0.0, 0.5);
    m_deltaSpin = makeSpin(this, "convolutionDeltaSpin", -255.0, 255.0, 0.0, 1.0);

    // Custom 3x3 kernel (row-major, identity by default)
    m_kernelGrid = new QWidget(this);
    auto *gridLayout = new QGridLayout(m_kernelGrid);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 9; ++i) {
        const double value = (i == 4) ? 1.0 : 0.0;
        auto *cell = makeSpin(m_kernelGrid,
                              QStringLiteral("convolutionKernelCell%1").arg(i).toUtf8(),
                              -99.0, 99.0, value, 0.1);
        m_kernelCells[size_t(i)] = cell;
        gridLayout->addWidget(cell, i / 3, i % 3);
    }

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("convolutionHint"));
    m_hint->setWordWrap(true);

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("convolutionApplyButton"));
    connect(applyButton, &QPushButton::clicked,
            this, &ConvolutionDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("类型:"), m_typeCombo);
    form->addRow(QStringLiteral("核大小:"), m_kernelSpin);
    form->addRow(QStringLiteral("SigmaX:"), m_sigmaXSpin);
    form->addRow(QStringLiteral("SigmaY:"), m_sigmaYSpin);
    form->addRow(QStringLiteral("卷积核(3×3):"), m_kernelGrid);
    form->addRow(QStringLiteral("Delta:"), m_deltaSpin);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_hint);
    layout->addWidget(applyButton);
    resize(360, 320);

    connect(m_typeCombo, &QComboBox::currentIndexChanged,
            this, [this] { updateParameterVisibility(); });
    updateParameterVisibility();
}

void ConvolutionDialog::updateParameterVisibility()
{
    switch (m_typeCombo->currentIndex()) {
    case 0:  // 均值
        m_kernelSpin->setEnabled(true);
        m_sigmaXSpin->setEnabled(false);
        m_sigmaYSpin->setEnabled(false);
        m_kernelGrid->setEnabled(false);
        m_deltaSpin->setEnabled(false);
        m_hint->setText(QStringLiteral("核内取平均,核越大越模糊。"));
        break;
    case 2:  // 中值
        m_kernelSpin->setEnabled(true);
        m_sigmaXSpin->setEnabled(false);
        m_sigmaYSpin->setEnabled(false);
        m_kernelGrid->setEnabled(false);
        m_deltaSpin->setEnabled(false);
        m_hint->setText(QStringLiteral("取邻域中值,去椒盐噪声;核大小 3–5。"));
        break;
    case 3:  // 自定义
        m_kernelSpin->setEnabled(false);
        m_sigmaXSpin->setEnabled(false);
        m_sigmaYSpin->setEnabled(false);
        m_kernelGrid->setEnabled(true);
        m_deltaSpin->setEnabled(true);
        m_hint->setText(QStringLiteral("3×3 自定义卷积核(默认恒等);Delta 加到每个结果像素。"));
        break;
    case 1:  // 高斯
    default:
        m_kernelSpin->setEnabled(true);
        m_sigmaXSpin->setEnabled(true);
        m_sigmaYSpin->setEnabled(true);
        m_kernelGrid->setEnabled(false);
        m_deltaSpin->setEnabled(false);
        m_hint->setText(QStringLiteral("高斯核;Sigma 为 0 时由核大小自动推导。"));
        break;
    }
}

BlurParams ConvolutionDialog::blurParams() const
{
    BlurParams params;
    switch (m_typeCombo->currentIndex()) {
    case 0:
        params.type = BlurType::Box;
        break;
    case 2:
        params.type = BlurType::Median;
        break;
    case 3:
        params.type = BlurType::Custom;
        break;
    case 1:
    default:
        params.type = BlurType::Gaussian;
        break;
    }
    params.kernelSize = m_kernelSpin->value();
    params.sigmaX = m_sigmaXSpin->value();
    params.sigmaY = m_sigmaYSpin->value();
    params.delta = m_deltaSpin->value();
    for (int i = 0; i < 9; ++i)
        params.kernel[size_t(i)] = m_kernelCells[size_t(i)]->value();
    return params;
}

QString ConvolutionDialog::currentTypeName() const
{
    return m_typeCombo->currentText();
}

} // namespace chameleon
