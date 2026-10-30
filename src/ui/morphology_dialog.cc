#include "ui/morphology_dialog.h"

#include <algorithm>

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chameleon {

namespace {

const int kBorderTypes[5] = {cv::BORDER_CONSTANT, cv::BORDER_REPLICATE,
                             cv::BORDER_REFLECT, cv::BORDER_REFLECT_101,
                             cv::BORDER_WRAP};

QSpinBox *makeSpin(QWidget *parent, const char *name, int min, int max,  // NOLINT(bugprone-easily-swappable-parameters) - fixed order
                   int value)
{
    auto *spin = new QSpinBox(parent);
    spin->setObjectName(QString::fromUtf8(name));
    spin->setRange(min, max);
    spin->setValue(value);
    return spin;
}

} // namespace

MorphologyDialog::MorphologyDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("形态学分析"));
    setObjectName(QStringLiteral("morphologyDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_operationCombo = new QComboBox(this);
    m_operationCombo->setObjectName(
        QStringLiteral("morphologyOperationCombo"));
    m_operationCombo->addItem(QStringLiteral("膨胀(dilate)"));
    m_operationCombo->addItem(QStringLiteral("腐蚀(erode)"));
    m_operationCombo->addItem(QStringLiteral("开运算(open)"));
    m_operationCombo->addItem(QStringLiteral("闭运算(close)"));
    m_operationCombo->addItem(QStringLiteral("基本梯度(膨胀-腐蚀)"));
    m_operationCombo->addItem(QStringLiteral("内梯度(原图-腐蚀)"));
    m_operationCombo->addItem(QStringLiteral("外梯度(膨胀-原图)"));

    m_methodCombo = new QComboBox(this);
    m_methodCombo->setObjectName(QStringLiteral("morphologyMethodCombo"));
    m_methodCombo->addItem(QStringLiteral("morphologyEx"));
    m_methodCombo->addItem(QStringLiteral("膨胀+腐蚀组合"));

    m_shapeCombo = new QComboBox(this);
    m_shapeCombo->setObjectName(QStringLiteral("morphologyShapeCombo"));
    m_shapeCombo->addItem(QStringLiteral("矩形"));
    m_shapeCombo->addItem(QStringLiteral("十字"));
    m_shapeCombo->addItem(QStringLiteral("椭圆"));

    m_widthSpin = makeSpin(this, "morphologyWidthSpin", 1, 255, 3);
    m_heightSpin = makeSpin(this, "morphologyHeightSpin", 1, 255, 3);

    m_anchorXSpin = makeSpin(this, "morphologyAnchorXSpin", -1, 255, -1);
    m_anchorYSpin = makeSpin(this, "morphologyAnchorYSpin", -1, 255, -1);

    m_iterationsSpin = makeSpin(this, "morphologyIterationsSpin", 1, 100, 1);

    m_borderCombo = new QComboBox(this);
    m_borderCombo->setObjectName(QStringLiteral("morphologyBorderCombo"));
    m_borderCombo->addItem(QStringLiteral("恒定值"));
    m_borderCombo->addItem(QStringLiteral("复制"));
    m_borderCombo->addItem(QStringLiteral("反射"));
    m_borderCombo->addItem(QStringLiteral("反射 101"));
    m_borderCombo->addItem(QStringLiteral("环绕"));

    m_borderValueSpin = makeSpin(this, "morphologyBorderValueSpin", 0, 255, 0);

    auto *sizeRow = new QWidget(this);
    auto *sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->addWidget(m_widthSpin);
    sizeLayout->addWidget(new QLabel(QStringLiteral("×"), sizeRow));
    sizeLayout->addWidget(m_heightSpin);

    auto *anchorRow = new QWidget(this);
    auto *anchorLayout = new QHBoxLayout(anchorRow);
    anchorLayout->setContentsMargins(0, 0, 0, 0);
    anchorLayout->addWidget(m_anchorXSpin);
    anchorLayout->addWidget(new QLabel(QStringLiteral(","), anchorRow));
    anchorLayout->addWidget(m_anchorYSpin);

    auto *hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setText(QStringLiteral(
        "对当前图片(或选区)执行形态学操作:膨胀扩大亮区、腐蚀缩小亮区;"
        "基本梯度 = 膨胀−腐蚀,内梯度 = 原图−腐蚀,外梯度 = 膨胀−原图。"
        "锚点 (-1, -1) 表示核中心;边界值仅在“恒定值”模式下生效。"
        "支持选区,可撤销。"));

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("morphologyApplyButton"));
    connect(applyButton, &QPushButton::clicked, this,
            &MorphologyDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("操作:"), m_operationCombo);
    form->addRow(QStringLiteral("实现方式:"), m_methodCombo);
    form->addRow(QStringLiteral("核形状:"), m_shapeCombo);
    form->addRow(QStringLiteral("核大小:"), sizeRow);
    form->addRow(QStringLiteral("锚点:"), anchorRow);
    form->addRow(QStringLiteral("迭代次数:"), m_iterationsSpin);
    form->addRow(QStringLiteral("边界类型:"), m_borderCombo);
    form->addRow(QStringLiteral("边界值:"), m_borderValueSpin);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(applyButton);
    resize(480, 390);

    connect(m_operationCombo, &QComboBox::currentIndexChanged, this,
            [this] { updateMethodEnabled(); });
    updateMethodEnabled();
}

void MorphologyDialog::updateMethodEnabled()
{
    // The implementation choice only matters for open/close
    const int index = m_operationCombo->currentIndex();
    m_methodCombo->setEnabled(index == 2 || index == 3);
}

MorphologyParams MorphologyDialog::morphologyParams() const
{
    MorphologyParams params;
    switch (m_operationCombo->currentIndex()) {
    case 1:
        params.operation = MorphologyOperation::Erode;
        break;
    case 2:
        params.operation = MorphologyOperation::Open;
        break;
    case 3:
        params.operation = MorphologyOperation::Close;
        break;
    case 4:
        params.operation = MorphologyOperation::BasicGradient;
        break;
    case 5:
        params.operation = MorphologyOperation::InternalGradient;
        break;
    case 6:
        params.operation = MorphologyOperation::ExternalGradient;
        break;
    default:
        params.operation = MorphologyOperation::Dilate;
        break;
    }
    params.method = m_methodCombo->currentIndex() == 1
            ? MorphologyMethod::Combined
            : MorphologyMethod::MorphologyEx;
    switch (m_shapeCombo->currentIndex()) {
    case 1:
        params.shape = MorphologyShape::Cross;
        break;
    case 2:
        params.shape = MorphologyShape::Ellipse;
        break;
    default:
        params.shape = MorphologyShape::Rect;
        break;
    }
    params.kernelWidth = m_widthSpin->value();
    params.kernelHeight = m_heightSpin->value();
    params.anchor = cv::Point(m_anchorXSpin->value(), m_anchorYSpin->value());
    params.iterations = m_iterationsSpin->value();
    const int borderIndex =
        std::clamp(m_borderCombo->currentIndex(), 0, 4);
    params.borderType = kBorderTypes[borderIndex];
    params.borderValue = m_borderValueSpin->value();
    return params;
}

} // namespace chameleon
