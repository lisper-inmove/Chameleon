#include "ui/threshold_dialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace chameleon {

ThresholdDialog::ThresholdDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("阈值化分割"));
    setObjectName(QStringLiteral("thresholdDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->setObjectName(QStringLiteral("thresholdMethodCombo"));
    m_methodCombo->addItem(QStringLiteral("固定阈值"));
    m_methodCombo->addItem(QStringLiteral("大津法(自动)"));
    m_methodCombo->addItem(QStringLiteral("三角法(自动)"));
    m_methodCombo->addItem(QStringLiteral("自适应阈值"));

    m_threshSpin = new QDoubleSpinBox(this);
    m_threshSpin->setObjectName(QStringLiteral("thresholdValueSpin"));
    m_threshSpin->setRange(0.0, 255.0);
    m_threshSpin->setSingleStep(1.0);
    m_threshSpin->setValue(128.0);

    m_maxvalSpin = new QDoubleSpinBox(this);
    m_maxvalSpin->setObjectName(QStringLiteral("thresholdMaxvalSpin"));
    m_maxvalSpin->setRange(0.0, 255.0);
    m_maxvalSpin->setSingleStep(1.0);
    m_maxvalSpin->setValue(255.0);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->setObjectName(QStringLiteral("thresholdTypeCombo"));
    m_typeCombo->addItem(QStringLiteral("二值化"));
    m_typeCombo->addItem(QStringLiteral("反二值化"));
    m_typeCombo->addItem(QStringLiteral("截断"));
    m_typeCombo->addItem(QStringLiteral("低于阈值置零"));
    m_typeCombo->addItem(QStringLiteral("高于阈值置零"));

    m_adaptiveMethodCombo = new QComboBox(this);
    m_adaptiveMethodCombo->setObjectName(QStringLiteral("thresholdAdaptiveMethodCombo"));
    m_adaptiveMethodCombo->addItem(QStringLiteral("邻域均值"));
    m_adaptiveMethodCombo->addItem(QStringLiteral("邻域高斯"));

    m_blockSizeSpin = new QSpinBox(this);
    m_blockSizeSpin->setObjectName(QStringLiteral("thresholdBlockSizeSpin"));
    m_blockSizeSpin->setRange(3, 99);
    m_blockSizeSpin->setSingleStep(2);  // odd sizes
    m_blockSizeSpin->setValue(11);

    m_cSpin = new QDoubleSpinBox(this);
    m_cSpin->setObjectName(QStringLiteral("thresholdCSpin"));
    m_cSpin->setRange(-50.0, 50.0);
    m_cSpin->setSingleStep(1.0);
    m_cSpin->setValue(2.0);

    m_computedLabel = new QLabel(this);
    m_computedLabel->setObjectName(QStringLiteral("thresholdComputedLabel"));
    m_computedLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("thresholdHint"));
    m_hint->setWordWrap(true);

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("thresholdApplyButton"));
    connect(applyButton, &QPushButton::clicked,
            this, &ThresholdDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("算法:"), m_methodCombo);
    form->addRow(QStringLiteral("阈值:"), m_threshSpin);
    form->addRow(QStringLiteral("最大值:"), m_maxvalSpin);
    form->addRow(QStringLiteral("类型:"), m_typeCombo);
    form->addRow(QStringLiteral("邻域方法:"), m_adaptiveMethodCombo);
    form->addRow(QStringLiteral("邻域大小:"), m_blockSizeSpin);
    form->addRow(QStringLiteral("常数 C:"), m_cSpin);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_computedLabel);
    layout->addWidget(m_hint);
    layout->addWidget(applyButton);
    resize(420, 340);

    connect(m_methodCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateParameterVisibility();
        updateHint();
    });
    connect(m_typeCombo, &QComboBox::currentIndexChanged,
            this, [this] { updateHint(); });
    updateParameterVisibility();
    updateHint();
}

void ThresholdDialog::updateParameterVisibility()
{
    const int method = m_methodCombo->currentIndex();
    const bool adaptive = method == 3;
    const bool automatic = method == 1 || method == 2;

    m_threshSpin->setEnabled(method == 0);  // manual value only for Fixed
    m_adaptiveMethodCombo->setEnabled(adaptive);
    m_blockSizeSpin->setEnabled(adaptive);
    m_cSpin->setEnabled(adaptive);
    m_computedLabel->setVisible(automatic);

    // adaptiveThreshold only supports the two binary types
    if (auto *model = qobject_cast<QStandardItemModel *>(m_typeCombo->model())) {
        for (int i = 2; i < m_typeCombo->count(); ++i)
            model->item(i)->setEnabled(!adaptive);
    }
    if (adaptive && m_typeCombo->currentIndex() > 1)
        m_typeCombo->setCurrentIndex(0);
}

void ThresholdDialog::updateHint()
{
    switch (m_methodCombo->currentIndex()) {
    case 1:
        m_hint->setText(QStringLiteral(
            "大津法:按最大类间方差自动确定阈值并二值化,忽略手动阈值;"
            "彩色图按亮度计算,结果复制回彩色。"));
        break;
    case 2:
        m_hint->setText(QStringLiteral(
            "三角法:按直方图几何形状自动确定阈值,适合单峰分布;忽略手动阈值。"));
        break;
    case 3:
        m_hint->setText(QStringLiteral(
            "自适应阈值:每个像素与邻域(大小需为奇数)加权均值减去 C 比较,"
            "适合光照不均;仅支持二值化/反二值化。"));
        break;
    case 0:
    default:
        m_hint->setText(QStringLiteral(
            "固定阈值:大于阈值置为最大值,其余置 0;阈值在 0–255 之间。"));
        break;
    }
}

ThresholdParams ThresholdDialog::thresholdParams() const
{
    ThresholdParams params;
    switch (m_methodCombo->currentIndex()) {
    case 1:
        params.method = ThresholdMethod::Otsu;
        break;
    case 2:
        params.method = ThresholdMethod::Triangle;
        break;
    case 3:
        params.method = ThresholdMethod::Adaptive;
        break;
    case 0:
    default:
        params.method = ThresholdMethod::Fixed;
        break;
    }
    params.thresh = m_threshSpin->value();
    params.maxval = m_maxvalSpin->value();
    switch (m_typeCombo->currentIndex()) {
    case 1:
        params.type = ThresholdType::BinaryInv;
        break;
    case 2:
        params.type = ThresholdType::Trunc;
        break;
    case 3:
        params.type = ThresholdType::ToZero;
        break;
    case 4:
        params.type = ThresholdType::ToZeroInv;
        break;
    case 0:
    default:
        params.type = ThresholdType::Binary;
        break;
    }
    if (params.method == ThresholdMethod::Adaptive
            && params.type != ThresholdType::Binary
            && params.type != ThresholdType::BinaryInv) {
        params.type = ThresholdType::Binary;  // unsupported by adaptiveThreshold
    }
    params.adaptiveMethod = m_adaptiveMethodCombo->currentIndex() == 1
                                ? AdaptiveThresholdMethod::Gaussian
                                : AdaptiveThresholdMethod::Mean;
    params.blockSize = m_blockSizeSpin->value();
    params.c = m_cSpin->value();
    return params;
}

void ThresholdDialog::setComputedThreshold(double value)
{
    m_computedLabel->setText(
        QStringLiteral("计算阈值:%1").arg(QString::number(value, 'f', 1)));
}

} // namespace chameleon
