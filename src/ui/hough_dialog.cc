#include "ui/hough_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace chameleon {

namespace {

QDoubleSpinBox *makeDoubleSpin(QWidget *parent, const char *name, double min,
                               double max, double value,  // NOLINT(bugprone-easily-swappable-parameters) - fixed order
                               int decimals)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setObjectName(QString::fromUtf8(name));
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value);
    return spin;
}

QSpinBox *makeIntSpin(QWidget *parent, const char *name, int min, int max,  // NOLINT(bugprone-easily-swappable-parameters) - fixed order
                      int value)
{
    auto *spin = new QSpinBox(parent);
    spin->setObjectName(QString::fromUtf8(name));
    spin->setRange(min, max);
    spin->setValue(value);
    return spin;
}

} // namespace

HoughDialog::HoughDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("霍夫变换"));
    setObjectName(QStringLiteral("houghDialog"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->setObjectName(QStringLiteral("houghModeCombo"));
    m_modeCombo->addItem(QStringLiteral("霍夫直线(HoughLines)"));
    m_modeCombo->addItem(QStringLiteral("霍夫圆(HoughCircles)"));

    // ------------------------------------------------------------------
    // cv::HoughLines
    // ------------------------------------------------------------------
    m_lineGroup = new QGroupBox(QStringLiteral("直线参数"), this);
    m_rhoSpin = makeDoubleSpin(m_lineGroup, "houghRhoSpin", 0.1, 100.0, 1.0, 2);
    m_thetaSpin =
        makeDoubleSpin(m_lineGroup, "houghThetaSpin", 0.1, 10.0, 1.0, 2);
    m_thresholdSpin =
        makeIntSpin(m_lineGroup, "houghThresholdSpin", 1, 5000, 150);
    m_srnSpin = makeDoubleSpin(m_lineGroup, "houghSrnSpin", 0.0, 100.0, 0.0, 2);
    m_stnSpin = makeDoubleSpin(m_lineGroup, "houghStnSpin", 0.0, 100.0, 0.0, 2);
    m_minThetaSpin =
        makeDoubleSpin(m_lineGroup, "houghMinThetaSpin", -180.0, 360.0, 0.0, 1);
    m_maxThetaSpin = makeDoubleSpin(m_lineGroup, "houghMaxThetaSpin", -180.0,
                                    360.0, 180.0, 1);
    m_canny1Spin = makeIntSpin(m_lineGroup, "houghCanny1Spin", 0, 255, 100);
    m_canny2Spin = makeIntSpin(m_lineGroup, "houghCanny2Spin", 0, 255, 200);
    m_useEdgeValCheck =
        new QCheckBox(QStringLiteral("累加器使用边缘值(use_edgeval)"),
                      m_lineGroup);
    m_useEdgeValCheck->setObjectName(
        QStringLiteral("houghUseEdgeValCheck"));
    m_lineThicknessSpin =
        makeIntSpin(m_lineGroup, "houghLineThicknessSpin", 1, 10, 2);

    auto *angleRow = new QWidget(m_lineGroup);
    auto *angleLayout = new QHBoxLayout(angleRow);
    angleLayout->setContentsMargins(0, 0, 0, 0);
    angleLayout->addWidget(m_minThetaSpin);
    angleLayout->addWidget(new QLabel(QStringLiteral("~"), angleRow));
    angleLayout->addWidget(m_maxThetaSpin);

    auto *cannyRow = new QWidget(m_lineGroup);
    auto *cannyLayout = new QHBoxLayout(cannyRow);
    cannyLayout->setContentsMargins(0, 0, 0, 0);
    cannyLayout->addWidget(m_canny1Spin);
    cannyLayout->addWidget(new QLabel(QStringLiteral("/"), cannyRow));
    cannyLayout->addWidget(m_canny2Spin);

    auto *lineForm = new QFormLayout(m_lineGroup);
    lineForm->addRow(QStringLiteral("距离分辨率 ρ:"), m_rhoSpin);
    lineForm->addRow(QStringLiteral("角度分辨率 θ(°):"), m_thetaSpin);
    lineForm->addRow(QStringLiteral("累加阈值:"), m_thresholdSpin);
    lineForm->addRow(QStringLiteral("srn:"), m_srnSpin);
    lineForm->addRow(QStringLiteral("stn:"), m_stnSpin);
    lineForm->addRow(QStringLiteral("角度范围(°):"), angleRow);
    lineForm->addRow(QStringLiteral("Canny 阈值:"), cannyRow);
    lineForm->addRow(QString(), m_useEdgeValCheck);
    lineForm->addRow(QStringLiteral("线宽:"), m_lineThicknessSpin);

    // ------------------------------------------------------------------
    // cv::HoughCircles
    // ------------------------------------------------------------------
    m_circleGroup = new QGroupBox(QStringLiteral("圆参数"), this);
    m_methodCombo = new QComboBox(m_circleGroup);
    m_methodCombo->setObjectName(QStringLiteral("houghMethodCombo"));
    m_methodCombo->addItem(QStringLiteral("HOUGH_GRADIENT"));
    m_methodCombo->addItem(QStringLiteral("HOUGH_GRADIENT_ALT"));
    m_dpSpin = makeDoubleSpin(m_circleGroup, "houghDpSpin", 1.0, 10.0, 1.0, 2);
    m_minDistSpin =
        makeDoubleSpin(m_circleGroup, "houghMinDistSpin", 1.0, 2000.0, 20.0, 1);
    m_param1Spin =
        makeDoubleSpin(m_circleGroup, "houghParam1Spin", 0.0, 1000.0, 100.0, 1);
    m_param2Spin =
        makeDoubleSpin(m_circleGroup, "houghParam2Spin", 0.0, 1000.0, 30.0, 1);
    m_minRadiusSpin = makeIntSpin(m_circleGroup, "houghMinRadiusSpin", 0, 5000, 0);
    m_maxRadiusSpin = makeIntSpin(m_circleGroup, "houghMaxRadiusSpin", 0, 5000, 0);
    m_circleThicknessSpin =
        makeIntSpin(m_circleGroup, "houghCircleThicknessSpin", 1, 10, 2);

    auto *radiusRow = new QWidget(m_circleGroup);
    auto *radiusLayout = new QHBoxLayout(radiusRow);
    radiusLayout->setContentsMargins(0, 0, 0, 0);
    radiusLayout->addWidget(m_minRadiusSpin);
    radiusLayout->addWidget(new QLabel(QStringLiteral("~"), radiusRow));
    radiusLayout->addWidget(m_maxRadiusSpin);

    auto *circleForm = new QFormLayout(m_circleGroup);
    circleForm->addRow(QStringLiteral("方法:"), m_methodCombo);
    circleForm->addRow(QStringLiteral("dp:"), m_dpSpin);
    circleForm->addRow(QStringLiteral("最小圆心距:"), m_minDistSpin);
    circleForm->addRow(QStringLiteral("param1:"), m_param1Spin);
    circleForm->addRow(QStringLiteral("param2:"), m_param2Spin);
    circleForm->addRow(QStringLiteral("半径范围:"), radiusRow);
    circleForm->addRow(QStringLiteral("线宽:"), m_circleThicknessSpin);

    // ------------------------------------------------------------------
    // Results + apply
    // ------------------------------------------------------------------
    m_countLabel = new QLabel(QStringLiteral("检测到 -- 个目标"), this);
    m_countLabel->setObjectName(QStringLiteral("houghCountLabel"));

    m_resultTable = new QTableWidget(0, 2, this);
    m_resultTable->setObjectName(QStringLiteral("houghResultTable"));
    m_resultTable->setHorizontalHeaderLabels(
        {QStringLiteral("编号"), QStringLiteral("结果")});
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setMinimumHeight(170);

    auto *hint = new QLabel(this);
    hint->setWordWrap(true);
    hint->setText(QStringLiteral(
        "霍夫直线:Canny 边缘后调用 cv::HoughLines,检测到的直线贯穿整幅图像绘制。"
        "霍夫圆:直接调用 cv::HoughCircles。支持选区,可撤销。"));

    auto *applyButton = new QPushButton(QStringLiteral("应用"), this);
    applyButton->setObjectName(QStringLiteral("houghApplyButton"));
    connect(applyButton, &QPushButton::clicked, this,
            &HoughDialog::applyRequested);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_modeCombo);
    layout->addWidget(m_lineGroup);
    layout->addWidget(m_circleGroup);
    layout->addWidget(m_countLabel);
    layout->addWidget(m_resultTable, 1);
    layout->addWidget(hint);
    layout->addWidget(applyButton);
    resize(560, 760);

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateModeVisibility();
    });
    updateModeVisibility();
}

void HoughDialog::updateModeVisibility()
{
    const bool lines = mode() == HoughMode::Lines;
    m_lineGroup->setEnabled(lines);
    m_circleGroup->setEnabled(!lines);
}

HoughMode HoughDialog::mode() const
{
    return m_modeCombo->currentIndex() == 1 ? HoughMode::Circles
                                            : HoughMode::Lines;
}

HoughLinesParams HoughDialog::lineParams() const
{
    HoughLinesParams params;
    params.rho = m_rhoSpin->value();
    params.thetaDegrees = m_thetaSpin->value();
    params.threshold = m_thresholdSpin->value();
    params.srn = m_srnSpin->value();
    params.stn = m_stnSpin->value();
    params.minThetaDegrees = m_minThetaSpin->value();
    params.maxThetaDegrees = m_maxThetaSpin->value();
    params.cannyThreshold1 = m_canny1Spin->value();
    params.cannyThreshold2 = m_canny2Spin->value();
    params.useEdgeVal = m_useEdgeValCheck->isChecked();
    params.thickness = m_lineThicknessSpin->value();
    return params;
}

HoughCirclesParams HoughDialog::circleParams() const
{
    HoughCirclesParams params;
    params.method = m_methodCombo->currentIndex() == 1
            ? cv::HOUGH_GRADIENT_ALT
            : cv::HOUGH_GRADIENT;
    params.dp = m_dpSpin->value();
    params.minDist = m_minDistSpin->value();
    params.param1 = m_param1Spin->value();
    params.param2 = m_param2Spin->value();
    params.minRadius = m_minRadiusSpin->value();
    params.maxRadius = m_maxRadiusSpin->value();
    params.thickness = m_circleThicknessSpin->value();
    return params;
}

void HoughDialog::setHoughResult(const HoughResult &result, HoughMode mode)
{
    if (mode == HoughMode::Lines) {
        m_countLabel->setText(
            QStringLiteral("检测到 %1 条直线").arg(result.lines.size()));
        m_resultTable->setRowCount(int(result.lines.size()));
        for (int row = 0; row < int(result.lines.size()); ++row) {
            const HoughLineInfo &line = result.lines[size_t(row)];
            m_resultTable->setItem(
                row, 0, new QTableWidgetItem(QString::number(line.index)));
            m_resultTable->setItem(
                row, 1,
                new QTableWidgetItem(
                    QStringLiteral("ρ=%1 px θ=%2°")
                        .arg(line.rho, 0, 'f', 2)
                        .arg(line.thetaDegrees, 0, 'f', 2)));
        }
        return;
    }

    m_countLabel->setText(
        QStringLiteral("检测到 %1 个圆").arg(result.circles.size()));
    m_resultTable->setRowCount(int(result.circles.size()));
    for (int row = 0; row < int(result.circles.size()); ++row) {
        const HoughCircleInfo &circle = result.circles[size_t(row)];
        m_resultTable->setItem(
            row, 0, new QTableWidgetItem(QString::number(circle.index)));
        m_resultTable->setItem(
            row, 1,
            new QTableWidgetItem(
                QStringLiteral("圆心=(%1, %2) 半径=%3")
                    .arg(circle.center.x, 0, 'f', 1)
                    .arg(circle.center.y, 0, 'f', 1)
                    .arg(circle.radius, 0, 'f', 1)));
    }
}

} // namespace chameleon
