#include "ui/geometry_dialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chameleon {

namespace {

QDoubleSpinBox *makeDoubleSpin(QWidget *parent, const char *name, double min,
                               double max, double value)  // NOLINT(bugprone-easily-swappable-parameters) - fixed order
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setObjectName(QString::fromUtf8(name));
    spin->setRange(min, max);
    spin->setDecimals(0);
    spin->setValue(value);
    return spin;
}

} // namespace

GeometryDialog::GeometryDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("几何变换与裁剪"));
    setObjectName(QStringLiteral("geometryDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->setObjectName(QStringLiteral("geometryModeCombo"));
    m_modeCombo->addItem(QStringLiteral("裁剪(选区)"));
    m_modeCombo->addItem(QStringLiteral("旋转"));
    m_modeCombo->addItem(QStringLiteral("翻转"));
    m_modeCombo->addItem(QStringLiteral("缩放重采样"));
    m_modeCombo->addItem(QStringLiteral("透视矫正(四点)"));
    m_modeCombo->addItem(QStringLiteral("画布大小"));
    m_modeCombo->setCurrentIndex(1);  // 旋转

    // The range covers both conventions: -90 and its equivalent 270
    m_angleSpin = makeDoubleSpin(this, "geometryAngleSpin", -360.0, 360.0, 90.0);
    m_angleSpin->setSingleStep(1.0);

    auto *quick90 = new QPushButton(QStringLiteral("90°"), this);
    quick90->setObjectName(QStringLiteral("geometryQuick90Button"));
    connect(quick90, &QPushButton::clicked, this, [this] { m_angleSpin->setValue(90.0); });
    auto *quick180 = new QPushButton(QStringLiteral("180°"), this);
    quick180->setObjectName(QStringLiteral("geometryQuick180Button"));
    connect(quick180, &QPushButton::clicked, this, [this] { m_angleSpin->setValue(180.0); });
    auto *quick270 = new QPushButton(QStringLiteral("270°"), this);
    quick270->setObjectName(QStringLiteral("geometryQuick270Button"));
    connect(quick270, &QPushButton::clicked, this, [this] { m_angleSpin->setValue(270.0); });

    m_flipCombo = new QComboBox(this);
    m_flipCombo->setObjectName(QStringLiteral("geometryFlipCombo"));
    m_flipCombo->addItem(QStringLiteral("水平"));
    m_flipCombo->addItem(QStringLiteral("垂直"));
    m_flipCombo->addItem(QStringLiteral("水平 + 垂直"));

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setObjectName(QStringLiteral("geometryWidthSpin"));
    m_widthSpin->setRange(1, 16384);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setObjectName(QStringLiteral("geometryHeightSpin"));
    m_heightSpin->setRange(1, 16384);

    m_resampleCombo = new QComboBox(this);
    m_resampleCombo->setObjectName(QStringLiteral("geometryResampleCombo"));
    m_resampleCombo->addItem(QStringLiteral("最近邻"));
    m_resampleCombo->addItem(QStringLiteral("双线性"));
    m_resampleCombo->addItem(QStringLiteral("双三次"));
    m_resampleCombo->setCurrentIndex(1);

    // Four-point perspective quad (TL, TR, BR, BL)
    auto *quadWidget = new QWidget(this);
    auto *quadLayout = new QVBoxLayout(quadWidget);
    quadLayout->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 4; ++i) {
        auto *row = new QHBoxLayout;
        auto *label = new QLabel(QStringLiteral("点%1:").arg(i + 1), quadWidget);
        label->setFixedWidth(36);
        m_quadX[size_t(i)] = makeDoubleSpin(
            quadWidget, QStringLiteral("geometryQuadX%1Spin").arg(i).toUtf8(), 0.0, 16383.0, 0.0);
        m_quadY[size_t(i)] = makeDoubleSpin(
            quadWidget, QStringLiteral("geometryQuadY%1Spin").arg(i).toUtf8(), 0.0, 16383.0, 0.0);
        row->addWidget(label);
        row->addWidget(m_quadX[size_t(i)]);
        row->addWidget(m_quadY[size_t(i)]);
        quadLayout->addLayout(row);
    }

    auto *sizeRow = new QWidget(this);
    auto *sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->addWidget(m_widthSpin);
    sizeLayout->addWidget(new QLabel(QStringLiteral("×"), sizeRow));
    sizeLayout->addWidget(m_heightSpin);

    auto *angleRow = new QWidget(this);
    auto *angleLayout = new QHBoxLayout(angleRow);
    angleLayout->setContentsMargins(0, 0, 0, 0);
    angleLayout->addWidget(m_angleSpin);
    angleLayout->addWidget(quick90);
    angleLayout->addWidget(quick180);
    angleLayout->addWidget(quick270);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("geometryHint"));
    m_hint->setWordWrap(true);

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("geometryApplyButton"));
    connect(applyButton, &QPushButton::clicked,
            this, &GeometryDialog::applyRequested);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("操作:"), m_modeCombo);
    form->addRow(QStringLiteral("角度:"), angleRow);
    form->addRow(QStringLiteral("方向:"), m_flipCombo);
    form->addRow(QStringLiteral("尺寸:"), sizeRow);
    form->addRow(QStringLiteral("插值:"), m_resampleCombo);
    form->addRow(QStringLiteral("四点:"), quadWidget);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_hint);
    layout->addWidget(applyButton);
    resize(420, 380);

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateParameterVisibility();
        updateHint();
    });
    updateParameterVisibility();
    updateHint();
}

void GeometryDialog::updateParameterVisibility()
{
    const int mode = m_modeCombo->currentIndex();
    m_angleSpin->setEnabled(mode == 1);
    for (auto *button : findChildren<QPushButton *>())
        if (button->objectName().startsWith(QStringLiteral("geometryQuick")))
            button->setEnabled(mode == 1);
    m_flipCombo->setEnabled(mode == 2);
    m_widthSpin->setEnabled(mode == 3 || mode == 5);
    m_heightSpin->setEnabled(mode == 3 || mode == 5);
    m_resampleCombo->setEnabled(mode == 3);
    for (auto *spin : m_quadX)
        spin->setEnabled(mode == 4);
    for (auto *spin : m_quadY)
        spin->setEnabled(mode == 4);
}

void GeometryDialog::updateHint()
{
    switch (m_modeCombo->currentIndex()) {
    case 0:
        m_hint->setText(QStringLiteral(
            "裁剪:需要先框选感兴趣区域(右键拖动),裁剪结果成为新的原图基准。"));
        break;
    case 2:
        m_hint->setText(QStringLiteral("翻转:水平 / 垂直 / 双向镜像。"));
        break;
    case 3:
        m_hint->setText(QStringLiteral(
            "缩放重采样:指定目标宽高;最近邻最快、双线性均衡、双三次更平滑。"));
        break;
    case 4:
        m_hint->setText(QStringLiteral(
            "透视矫正:四点按 左上 → 右上 → 右下 → 左下 的顺序;默认取当前选区(无选区取整幅图)。"));
        break;
    case 5:
        m_hint->setText(QStringLiteral(
            "画布大小:图像保持左上角,扩大时以不透明黑色填充,缩小时裁掉右下。"));
        break;
    case 1:
    default:
        m_hint->setText(QStringLiteral(
            "旋转:正角度逆时针;90 的倍数走无损旋转,其余角度自动扩大画布。"));
        break;
    }
}

GeometryParams GeometryDialog::geometryParams() const
{
    GeometryParams params;
    switch (m_modeCombo->currentIndex()) {
    case 0:
        params.operation = GeometryOperation::Crop;
        break;
    case 2:
        params.operation = GeometryOperation::Flip;
        break;
    case 3:
        params.operation = GeometryOperation::Resize;
        break;
    case 4:
        params.operation = GeometryOperation::Perspective;
        break;
    case 5:
        params.operation = GeometryOperation::Canvas;
        break;
    case 1:
    default:
        params.operation = GeometryOperation::Rotate;
        break;
    }
    params.angle = m_angleSpin->value();
    switch (m_flipCombo->currentIndex()) {
    case 1:
        params.flip = FlipDirection::Vertical;
        break;
    case 2:
        params.flip = FlipDirection::Both;
        break;
    case 0:
    default:
        params.flip = FlipDirection::Horizontal;
        break;
    }
    params.width = m_widthSpin->value();
    params.height = m_heightSpin->value();
    switch (m_resampleCombo->currentIndex()) {
    case 0:
        params.resample = ResampleMethod::Nearest;
        break;
    case 2:
        params.resample = ResampleMethod::Cubic;
        break;
    case 1:
    default:
        params.resample = ResampleMethod::Linear;
        break;
    }
    for (int i = 0; i < 4; ++i) {
        params.quad[size_t(i)] = cv::Point2f(float(m_quadX[size_t(i)]->value()),
                                             float(m_quadY[size_t(i)]->value()));
    }
    return params;
}

void GeometryDialog::setDefaults(const QSize &imageSize, const QRect &selection)
{
    m_widthSpin->setValue(std::max(1, imageSize.width()));
    m_heightSpin->setValue(std::max(1, imageSize.height()));

    QRect quadRect = selection;
    if (!quadRect.isValid() || quadRect.isEmpty())
        quadRect = QRect(0, 0, imageSize.width(), imageSize.height());
    const double x0 = quadRect.left();
    const double y0 = quadRect.top();
    const double x1 = quadRect.right();
    const double y1 = quadRect.bottom();
    const double xs[4] = {x0, x1, x1, x0};  // TL, TR, BR, BL
    const double ys[4] = {y0, y0, y1, y1};
    for (int i = 0; i < 4; ++i) {
        m_quadX[size_t(i)]->setValue(xs[i]);
        m_quadY[size_t(i)]->setValue(ys[i]);
    }
}

} // namespace chameleon
