#include "ui/noise_dialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chameleon {

NoiseDialog::NoiseDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("噪声与去噪"));
    setObjectName(QStringLiteral("noiseDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    // --- add noise ---
    m_noiseTypeCombo = new QComboBox(this);
    m_noiseTypeCombo->setObjectName(QStringLiteral("noiseTypeCombo"));
    m_noiseTypeCombo->addItem(QStringLiteral("椒盐噪声"));
    m_noiseTypeCombo->addItem(QStringLiteral("高斯噪声"));

    m_proportionSpin = new QDoubleSpinBox(this);
    m_proportionSpin->setObjectName(QStringLiteral("noiseProportionSpin"));
    m_proportionSpin->setRange(0.1, 20.0);
    m_proportionSpin->setSingleStep(0.1);
    m_proportionSpin->setValue(2.0);
    m_proportionSpin->setSuffix(QStringLiteral(" %"));

    m_meanSpin = new QDoubleSpinBox(this);
    m_meanSpin->setObjectName(QStringLiteral("noiseMeanSpin"));
    m_meanSpin->setRange(-100.0, 100.0);
    m_meanSpin->setValue(0.0);

    m_sigmaSpin = new QDoubleSpinBox(this);
    m_sigmaSpin->setObjectName(QStringLiteral("noiseSigmaSpin"));
    m_sigmaSpin->setRange(0.0, 100.0);
    m_sigmaSpin->setValue(20.0);

    auto *addNoiseButton = new QPushButton(QStringLiteral("添加噪声"), this);
    addNoiseButton->setObjectName(QStringLiteral("addNoiseButton"));
    connect(addNoiseButton, &QPushButton::clicked,
            this, &NoiseDialog::addNoiseRequested);

    auto *noiseGroup = new QGroupBox(QStringLiteral("添加噪声"), this);
    auto *noiseForm = new QFormLayout(noiseGroup);
    noiseForm->addRow(QStringLiteral("类型:"), m_noiseTypeCombo);
    noiseForm->addRow(QStringLiteral("比例:"), m_proportionSpin);
    noiseForm->addRow(QStringLiteral("均值:"), m_meanSpin);
    noiseForm->addRow(QStringLiteral("标准差:"), m_sigmaSpin);
    noiseForm->addRow(QString(), addNoiseButton);

    // --- denoise ---
    m_denoiseTypeCombo = new QComboBox(this);
    m_denoiseTypeCombo->setObjectName(QStringLiteral("denoiseTypeCombo"));
    m_denoiseTypeCombo->addItem(QStringLiteral("均值滤波"));
    m_denoiseTypeCombo->addItem(QStringLiteral("中值滤波"));

    m_denoiseKernelSpin = new QSpinBox(this);
    m_denoiseKernelSpin->setObjectName(QStringLiteral("denoiseKernelSpin"));
    m_denoiseKernelSpin->setRange(3, 15);
    m_denoiseKernelSpin->setSingleStep(2);  // odd sizes
    m_denoiseKernelSpin->setValue(3);

    auto *denoiseButton = new QPushButton(QStringLiteral("去噪"), this);
    denoiseButton->setObjectName(QStringLiteral("denoiseButton"));
    connect(denoiseButton, &QPushButton::clicked,
            this, &NoiseDialog::denoiseRequested);

    auto *denoiseGroup = new QGroupBox(QStringLiteral("去噪"), this);
    auto *denoiseForm = new QFormLayout(denoiseGroup);
    denoiseForm->addRow(QStringLiteral("类型:"), m_denoiseTypeCombo);
    denoiseForm->addRow(QStringLiteral("核大小:"), m_denoiseKernelSpin);
    denoiseForm->addRow(QString(), denoiseButton);

    auto *hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setText(QStringLiteral(
        "椒盐噪声去掉孤立亮点/暗点,中值滤波最有效;高斯噪声用均值滤波平滑。"
        "去噪核越大越平滑(也会更模糊)。"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(noiseGroup);
    layout->addWidget(denoiseGroup);
    layout->addWidget(hint);
    resize(360, 380);

    connect(m_noiseTypeCombo, &QComboBox::currentIndexChanged,
            this, [this] { updateParameterVisibility(); });
    updateParameterVisibility();
}

void NoiseDialog::updateParameterVisibility()
{
    const bool saltPepper = saltPepperSelected();
    m_proportionSpin->setEnabled(saltPepper);
    m_meanSpin->setEnabled(!saltPepper);
    m_sigmaSpin->setEnabled(!saltPepper);
}

bool NoiseDialog::saltPepperSelected() const
{
    return m_noiseTypeCombo->currentIndex() == 0;
}

SaltPepperParams NoiseDialog::saltPepperParams() const
{
    SaltPepperParams params;
    params.proportion = m_proportionSpin->value() / 100.0;
    params.saltRatio = 0.5;
    return params;
}

GaussianNoiseParams NoiseDialog::gaussianNoiseParams() const
{
    GaussianNoiseParams params;
    params.mean = m_meanSpin->value();
    params.sigma = m_sigmaSpin->value();
    return params;
}

BlurParams NoiseDialog::denoiseParams() const
{
    BlurParams params;
    params.type = m_denoiseTypeCombo->currentIndex() == 1 ? BlurType::Median
                                                          : BlurType::Box;
    params.kernelSize = m_denoiseKernelSpin->value();
    return params;
}

} // namespace chameleon
